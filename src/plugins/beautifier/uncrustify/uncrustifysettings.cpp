/****************************************************************************
**
** Copyright (C) 2016 Lorenz Haas
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

#include "uncrustifysettings.h"

#include "uncrustifyconstants.h"

#include "../beautifierconstants.h"

#include <coreplugin/icore.h>

#include <utils/qtcprocess.h>

#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QXmlStreamWriter>

using namespace Utils;

namespace Beautifier {
namespace Internal {

const char USE_OTHER_FILES[]               = "useOtherFiles";
const char USE_HOME_FILE[]                 = "useHomeFile";
const char USE_SPECIFIC_CONFIG_FILE_PATH[] = "useSpecificConfigFile";
const char SPECIFIC_CONFIG_FILE_PATH[]     = "specificConfigFile";
const char USE_CUSTOM_STYLE[]              = "useCustomStyle";
const char CUSTOM_STYLE[]                  = "customStyle";
const char FORMAT_ENTIRE_FILE_FALLBACK[]   = "formatEntireFileFallback";
const char SETTINGS_NAME[]                 = "uncrustify";

UncrustifySettings::UncrustifySettings() :
    AbstractSettings(SETTINGS_NAME, ".cfg")
{
    setCommand("uncrustify");
    m_settings.insert(USE_OTHER_FILES, QVariant(true));
    m_settings.insert(USE_HOME_FILE, QVariant(false));
    m_settings.insert(USE_CUSTOM_STYLE, QVariant(false));
    m_settings.insert(USE_SPECIFIC_CONFIG_FILE_PATH, QVariant(false));
    m_settings.insert(CUSTOM_STYLE, QVariant());
    m_settings.insert(FORMAT_ENTIRE_FILE_FALLBACK, QVariant(true));
    m_settings.insert(SPECIFIC_CONFIG_FILE_PATH, QVariant());
    read();
}

UncrustifySettings::~UncrustifySettings() = default;

bool UncrustifySettings::useOtherFiles() const
{
    return m_settings.value(USE_OTHER_FILES).toBool();
}

void UncrustifySettings::setUseOtherFiles(bool useOtherFiles)
{
    m_settings.insert(USE_OTHER_FILES, QVariant(useOtherFiles));
}

bool UncrustifySettings::useHomeFile() const
{
    return m_settings.value(USE_HOME_FILE).toBool();
}

void UncrustifySettings::setUseHomeFile(bool useHomeFile)
{
    m_settings.insert(USE_HOME_FILE, QVariant(useHomeFile));
}

FilePath UncrustifySettings::specificConfigFile() const
{
    return FilePath::fromString(m_settings.value(SPECIFIC_CONFIG_FILE_PATH).toString());
}

void UncrustifySettings::setSpecificConfigFile(const FilePath &filePath)
{
    m_settings.insert(SPECIFIC_CONFIG_FILE_PATH, QVariant(filePath.toString()));
}

bool UncrustifySettings::useSpecificConfigFile() const
{
    return m_settings.value(USE_SPECIFIC_CONFIG_FILE_PATH).toBool();
}

void UncrustifySettings::setUseSpecificConfigFile(bool useConfigFile)
{
    m_settings.insert(USE_SPECIFIC_CONFIG_FILE_PATH, QVariant(useConfigFile));
}

bool UncrustifySettings::useCustomStyle() const
{
    return m_settings.value(USE_CUSTOM_STYLE).toBool();
}

void UncrustifySettings::setUseCustomStyle(bool useCustomStyle)
{
    m_settings.insert(USE_CUSTOM_STYLE, QVariant(useCustomStyle));
}

QString UncrustifySettings::customStyle() const
{
    return m_settings.value(CUSTOM_STYLE).toString();
}

void UncrustifySettings::setCustomStyle(const QString &customStyle)
{
    m_settings.insert(CUSTOM_STYLE, QVariant(customStyle));
}

bool UncrustifySettings::formatEntireFileFallback() const
{
    return m_settings.value(FORMAT_ENTIRE_FILE_FALLBACK).toBool();
}

void UncrustifySettings::setFormatEntireFileFallback(bool formatEntireFileFallback)
{
    m_settings.insert(FORMAT_ENTIRE_FILE_FALLBACK, QVariant(formatEntireFileFallback));
}

QString UncrustifySettings::documentationFilePath() const
{
    return (Core::ICore::userResourcePath() / Beautifier::Constants::SETTINGS_DIRNAME
                / Beautifier::Constants::DOCUMENTATION_DIRNAME / SETTINGS_NAME
            + ".xml")
        .toString();
}

void UncrustifySettings::createDocumentationFile() const
{
    QtcProcess process;
    process.setTimeoutS(2);
    process.setCommand({command(), {"--show-config"}});
    process.runBlocking();
    if (process.result() != ProcessResult::FinishedWithSuccess)
        return;

    QFile file(documentationFilePath());
    const QFileInfo fi(file);
    if (!fi.exists())
        fi.dir().mkpath(fi.absolutePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
        return;

    bool contextWritten = false;
    QXmlStreamWriter stream(&file);
    stream.setAutoFormatting(true);
    stream.writeStartDocument("1.0", true);
    stream.writeComment("Created " + QDateTime::currentDateTime().toString(Qt::ISODate));
    stream.writeStartElement(Constants::DOCUMENTATION_XMLROOT);

    const QStringList lines = process.allOutput().split(QLatin1Char('\n'));
    const int totalLines = lines.count();
    for (int i = 0; i < totalLines; ++i) {
        const QString &line = lines.at(i);
        if (line.startsWith('#') || line.trimmed().isEmpty())
            continue;

        const int firstSpace = line.indexOf(' ');
        const QString keyword = line.left(firstSpace);
        const QString options = line.right(line.size() - firstSpace).trimmed();
        QStringList docu;
        while (++i < totalLines) {
            const QString &subline = lines.at(i);
            if (line.startsWith('#') || subline.trimmed().isEmpty()) {
                const QString text = "<p><span class=\"option\">" + keyword
                        + "</span> <span class=\"param\">" + options
                        + "</span></p><p>" + docu.join(' ').toHtmlEscaped() + "</p>";
                stream.writeStartElement(Constants::DOCUMENTATION_XMLENTRY);
                stream.writeTextElement(Constants::DOCUMENTATION_XMLKEY, keyword);
                stream.writeTextElement(Constants::DOCUMENTATION_XMLDOC, text);
                stream.writeEndElement();
                contextWritten = true;
                break;
            } else {
                docu << subline;
            }
        }
    }

    stream.writeEndElement();
    stream.writeEndDocument();

    // An empty file causes error messages and a contextless file preventing this function to run
    // again in order to generate the documentation successfully. Thus delete the file.
    if (!contextWritten) {
        file.close();
        file.remove();
    }
}

static bool parseVersion(const QString &text, int &version)
{
    // The version in Uncrustify is printed like "uncrustify 0.62"
    const QRegularExpression rx("([0-9]{1})\\.([0-9]{2})");
    const QRegularExpressionMatch match = rx.match(text);
    if (!match.hasMatch())
        return false;

    const int major = match.captured(1).toInt() * 100;
    const int minor = match.captured(2).toInt();
    version = major + minor;
    return true;
}

void UncrustifySettings::updateVersion()
{
    m_versionProcess.reset(new QtcProcess);
    connect(m_versionProcess.get(), &QtcProcess::finished, this, [this] {
        if (m_versionProcess->exitStatus() == QProcess::NormalExit) {
            if (!parseVersion(QString::fromUtf8(m_versionProcess->readAllStandardOutput()), m_version))
                parseVersion(QString::fromUtf8(m_versionProcess->readAllStandardError()), m_version);
        }
        m_versionProcess.release()->deleteLater();
    });
    m_versionProcess->setCommand({ command(), { "--version" } });
    m_versionProcess->start();
}

} // namespace Internal
} // namespace Beautifier
