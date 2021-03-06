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

#ifndef FILE_DOWNLOADER_H_
#define FILE_DOWNLOADER_H_

#include <QObject>
#include <QByteArray>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>

namespace file_downloader {

/*!
 * \brief The FileDownloader class, used to download file from server
 */
class FileDownloader : public QObject {

    Q_OBJECT

  public:
    /*!
     * \brief FileDownloader constructor
     */
    FileDownloader();

    /*!
     * \brief FileDownloader destructor
     */
    virtual ~FileDownloader();

    /*!
    * \brief Method used to start download process
    * \param url - URL
    */
    virtual void StartDownload(const QUrl& url);

    /*!
     * \brief Get downloaded data
     * \param downloaded_data - Data that is downloaded
     * \return True if data is successfully fetched, false otherwise
     */
    virtual bool GetDownloadedData(QByteArray& downloaded_data);

  signals:
    /*!
     * \brief Signals that data is downloaded
     */
    void Downloaded();

    /*!
     * \brief Signal download progress
     * \param bytes_received - Number of received bytes
     * \param bytes_total - Total number of bytes
     */
    void DownloadProgress(const qint64& bytes_received, const qint64& bytes_total);

  private slots:
    /*!
     * \brief Set download progress slot
     * \param bytes_received - Number of received bytes
     * \param bytes_total - Total number of bytes
     */
    void SetDownloadProgress(qint64 bytes_received, qint64 bytes_total);

    /*!
     * \brief FileDownloaded slot, file downloading is finished
     */
    void FileDownloaded();

  private:
    QNetworkAccessManager net_access_manager_;  //!< Network access manager
    QNetworkReply *reply_;                      //!< Pointer to network reply
};

} // namespace file_downloader

#endif // FILE_DOWNLOADER_H_
