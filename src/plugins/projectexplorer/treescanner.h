/****************************************************************************
**
** Copyright (C) 2016 Alexander Drozdov.
** Contact: Alexander Drozdov (adrozdoff@gmail.com)
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

#pragma once

#include "projectexplorer_export.h"
#include "projectnodes.h"

#include <utils/fileutils.h>
#include <utils/mimeutils.h>

#include <QObject>
#include <QFuture>
#include <QFutureWatcher>

#include <functional>

namespace Core { class IVersionControl; }

namespace ProjectExplorer {

class PROJECTEXPLORER_EXPORT TreeScanner : public QObject
{
    Q_OBJECT

public:
    struct Result
    {
        std::shared_ptr<FolderNode> folderNode;
        QList<FileNode *> allFiles;
    };
    using Future = QFuture<Result>;
    using FutureWatcher = QFutureWatcher<Result>;
    using FutureInterface = QFutureInterface<Result>;

    using FileFilter = std::function<bool(const Utils::MimeType &, const Utils::FilePath &)>;
    using FileTypeFactory = std::function<ProjectExplorer::FileType(const Utils::MimeType &, const Utils::FilePath &)>;

    explicit TreeScanner(QObject *parent = nullptr);
    ~TreeScanner() override;

    // Start scanning in given directory
    bool asyncScanForFiles(const Utils::FilePath& directory);

    // Setup filter for ignored files
    void setFilter(FileFilter filter);

    // Setup factory for file types
    void setTypeFactory(FileTypeFactory factory);

    Future future() const;
    bool isFinished() const;

    // Takes not-owning result
    Result result() const;
    // Takes owning of result
    Result release();
    // Clear scan results
    void reset();

    // Standard filters helpers
    static bool isWellKnownBinary(const Utils::MimeType &mimeType, const Utils::FilePath &fn);
    static bool isMimeBinary(const Utils::MimeType &mimeType, const Utils::FilePath &fn);

    // Standard file factory
    static ProjectExplorer::FileType genericFileType(const Utils::MimeType &mdb, const Utils::FilePath& fn);

signals:
    void finished();

private:
    static void scanForFiles(FutureInterface &fi, const Utils::FilePath &directory,
                             const FileFilter &filter, const FileTypeFactory &factory);

private:
    FileFilter m_filter;
    FileTypeFactory m_factory;

    FutureWatcher m_futureWatcher;
    Future m_scanFuture;
};

} // namespace ProjectExplorer
