/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#include "changeselectiondialog.h"
#include "logchangedialog.h"
#include "gitclient.h"
#include "ui_changeselectiondialog.h"

#include <coreplugin/vcsmanager.h>

#include <utils/pathchooser.h>
#include <utils/qtcprocess.h>
#include <utils/theme/theme.h>

#include <vcsbase/vcscommand.h>

#include <QCompleter>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStringListModel>
#include <QTimer>

using namespace Utils;

namespace Git {
namespace Internal {

ChangeSelectionDialog::ChangeSelectionDialog(const FilePath &workingDirectory, Id id,
                                             QWidget *parent) :
    QDialog(parent), m_ui(new Ui::ChangeSelectionDialog)
{
    m_gitExecutable = GitClient::instance()->vcsBinary();
    m_ui->setupUi(this);
    m_ui->workingDirectoryChooser->setExpectedKind(PathChooser::ExistingDirectory);
    m_ui->workingDirectoryChooser->setPromptDialogTitle(tr("Select Git Directory"));
    m_ui->workingDirectoryChooser->setFilePath(workingDirectory);
    m_gitEnvironment = GitClient::instance()->processEnvironment();
    m_ui->changeNumberEdit->setFocus();
    m_ui->changeNumberEdit->selectAll();

    connect(m_ui->changeNumberEdit, &CompletingLineEdit::textChanged,
            this, &ChangeSelectionDialog::changeTextChanged);
    connect(m_ui->workingDirectoryChooser, &PathChooser::pathChanged,
            this, &ChangeSelectionDialog::recalculateDetails);
    connect(m_ui->workingDirectoryChooser, &PathChooser::pathChanged,
            this, &ChangeSelectionDialog::recalculateCompletion);
    connect(m_ui->selectFromHistoryButton, &QPushButton::clicked,
            this, &ChangeSelectionDialog::selectCommitFromRecentHistory);
    connect(m_ui->showButton, &QPushButton::clicked,
            this, std::bind(&ChangeSelectionDialog::acceptCommand, this, Show));
    connect(m_ui->cherryPickButton, &QPushButton::clicked,
            this, std::bind(&ChangeSelectionDialog::acceptCommand, this, CherryPick));
    connect(m_ui->revertButton, &QPushButton::clicked,
            this, std::bind(&ChangeSelectionDialog::acceptCommand, this, Revert));
    connect(m_ui->checkoutButton, &QPushButton::clicked,
            this, std::bind(&ChangeSelectionDialog::acceptCommand, this, Checkout));
    connect(m_ui->archiveButton, &QPushButton::clicked,
            this, std::bind(&ChangeSelectionDialog::acceptCommand, this, Archive));

    if (id == "Git.Revert")
        m_ui->revertButton->setDefault(true);
    else if (id == "Git.CherryPick")
        m_ui->cherryPickButton->setDefault(true);
    else if (id == "Git.Checkout")
        m_ui->checkoutButton->setDefault(true);
    else if (id == "Git.Archive")
        m_ui->archiveButton->setDefault(true);
    else
        m_ui->showButton->setDefault(true);
    m_changeModel = new QStringListModel(this);
    auto changeCompleter = new QCompleter(m_changeModel, this);
    m_ui->changeNumberEdit->setCompleter(changeCompleter);
    changeCompleter->setCaseSensitivity(Qt::CaseInsensitive);

    recalculateDetails();
    recalculateCompletion();
}

ChangeSelectionDialog::~ChangeSelectionDialog()
{
    delete m_ui;
}

QString ChangeSelectionDialog::change() const
{
    return m_ui->changeNumberEdit->text().trimmed();
}

void ChangeSelectionDialog::selectCommitFromRecentHistory()
{
    FilePath workingDir = workingDirectory();
    if (workingDir.isEmpty())
        return;

    QString commit = change();
    int tilde = commit.indexOf('~');
    if (tilde != -1)
        commit.truncate(tilde);
    LogChangeDialog dialog(false, this);
    dialog.setWindowTitle(tr("Select Commit"));

    dialog.runDialog(workingDir, commit, LogChangeWidget::IncludeRemotes);

    if (dialog.result() == QDialog::Rejected || dialog.commitIndex() == -1)
        return;

    m_ui->changeNumberEdit->setText(dialog.commit());
}

FilePath ChangeSelectionDialog::workingDirectory() const
{
    const FilePath workingDir = m_ui->workingDirectoryChooser->filePath();
    if (workingDir.isEmpty() || !workingDir.exists())
        return {};

    return Core::VcsManager::findTopLevelForDirectory(workingDir);
}

ChangeCommand ChangeSelectionDialog::command() const
{
    return m_command;
}

void ChangeSelectionDialog::acceptCommand(ChangeCommand command)
{
    m_command = command;
    QDialog::accept();
}

//! Set commit message in details
void ChangeSelectionDialog::setDetails()
{
    Theme *theme = creatorTheme();

    QPalette palette;
    if (m_process->result() == ProcessResult::FinishedWithSuccess) {
        m_ui->detailsText->setPlainText(m_process->stdOut());
        palette.setColor(QPalette::Text, theme->color(Theme::TextColorNormal));
        m_ui->changeNumberEdit->setPalette(palette);
    } else {
        m_ui->detailsText->setPlainText(tr("Error: Unknown reference"));
        palette.setColor(QPalette::Text, theme->color(Theme::TextColorError));
        m_ui->changeNumberEdit->setPalette(palette);
        enableButtons(false);
    }
}

void ChangeSelectionDialog::enableButtons(bool b)
{
    m_ui->showButton->setEnabled(b);
    m_ui->cherryPickButton->setEnabled(b);
    m_ui->revertButton->setEnabled(b);
    m_ui->checkoutButton->setEnabled(b);
}

void ChangeSelectionDialog::recalculateCompletion()
{
    const FilePath workingDir = workingDirectory();
    if (workingDir == m_oldWorkingDir)
        return;
    m_oldWorkingDir = workingDir;
    m_changeModel->setStringList(QStringList());

    if (workingDir.isEmpty())
        return;

    GitClient *client = GitClient::instance();
    VcsBase::VcsCommand *command = client->asyncForEachRefCmd(
                workingDir, {"--format=%(refname:short)"});
    connect(this, &QObject::destroyed, command, &VcsBase::VcsCommand::abort);
    connect(command, &VcsBase::VcsCommand::stdOutText, [this](const QString &output) {
        m_changeModel->setStringList(output.split('\n'));
    });
}

void ChangeSelectionDialog::recalculateDetails()
{
    enableButtons(true);

    const FilePath workingDir = workingDirectory();
    if (workingDir.isEmpty()) {
        m_ui->detailsText->setPlainText(tr("Error: Bad working directory."));
        return;
    }

    const QString ref = change();
    if (ref.isEmpty()) {
        m_ui->detailsText->clear();
        return;
    }

    m_process.reset(new QtcProcess);
    m_process->setWorkingDirectory(workingDir);
    m_process->setEnvironment(m_gitEnvironment);
    m_process->setCommand({m_gitExecutable, {"show", "--decorate", "--stat=80", ref}});

    connect(m_process.get(), &QtcProcess::finished, this, &ChangeSelectionDialog::setDetails);

    m_process->start();
    if (!m_process->waitForStarted())
        m_ui->detailsText->setPlainText(tr("Error: Could not start Git."));
    else
        m_ui->detailsText->setPlainText(tr("Fetching commit data..."));
}

void ChangeSelectionDialog::changeTextChanged(const QString &text)
{
    if (QCompleter *comp = m_ui->changeNumberEdit->completer()) {
        if (text.isEmpty() && !comp->popup()->isVisible()) {
            comp->setCompletionPrefix(text);
            QTimer::singleShot(0, comp, [comp]{ comp->complete(); });
        }
    }
    recalculateDetails();
}

} // Internal
} // Git
