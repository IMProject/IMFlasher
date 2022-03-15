/****************************************************************************
 *
 *   Copyright (c) 2022 IMProject Development Team. All rights reserved.
 *   Authors: Igor Misic <igy1000mb@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name IMProject nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

#include "file_downloader.h"

namespace file_downloader {

FileDownloader::FileDownloader() = default;
FileDownloader::~FileDownloader() = default;

void FileDownloader::StartDownload(const QUrl& url)
{
    QNetworkRequest request(url);
    request.setSslConfiguration(QSslConfiguration::defaultConfiguration());
    reply_ = net_access_manager_.get(request);
    connect(reply_, &QNetworkReply::downloadProgress,this, &FileDownloader::SetDownloadProgress);
    connect(reply_, &QNetworkReply::finished, this, &FileDownloader::FileDownloaded);
}

void FileDownloader::SetDownloadProgress(qint64 bytes_received, qint64 bytes_total)
{
    emit DownloadProgress(bytes_received, bytes_total);
}

void FileDownloader::FileDownloaded() {
    emit Downloaded();
}

bool FileDownloader::GetDownloadedData(QByteArray& downloaded_data) {

    bool success = false;

    if(reply_ != nullptr) {
        downloaded_data = reply_->readAll();
        reply_->deleteLater();
        success = true;
    }

    return success;
}

} // namespace file_downloader
