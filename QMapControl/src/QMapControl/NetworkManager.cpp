/*
*
* This file is part of QMapControl,
* an open-source cross-platform map widget
*
* Copyright (C) 2007 - 2008 Kai Winter
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU Lesser General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with QMapControl. If not, see <http://www.gnu.org/licenses/>.
*
* Contact e-mail: kaiwinter@gmx.de
* Program URL   : http://qmapcontrol.sourceforge.net/
*
*/

#include "NetworkManager.h"

// Qt includes.
#include <QMutexLocker>
#include <QImageReader>
#include <QAbstractNetworkCache>

namespace qmapcontrol
{
    const int kReplyTimeout_s = 30;
    const int kReplyTimeoutCheckInterval_s = 5;

    NetworkManager::NetworkManager(QObject* parent)
        : QObject(parent)
    {
        // Connect signal/slot to handle proxy authentication.
        connect(&m_accessManager, &QNetworkAccessManager::proxyAuthenticationRequired, this, &NetworkManager::proxyAuthenticationRequired);

        // Connect signal/slot to handle finished downloads.
        connect(&m_accessManager, &QNetworkAccessManager::finished, this, &NetworkManager::downloadFinished);

        // Check periodically for timeouted network requests
        m_timeoutTimer.setInterval(kReplyTimeoutCheckInterval_s * 1000);
        connect(&m_timeoutTimer, &QTimer::timeout, this, &NetworkManager::checkReplyTimeouts);
    }

    NetworkManager::~NetworkManager()
    {
        // Ensure all download queues are aborted.
        abortDownloads();
    }

    void NetworkManager::setProxy(const QNetworkProxy& proxy, const QString& userName, const QString& password)
    {
        m_proxyUserName = userName;
        m_proxyPassword = password;

        // Set the proxy on the network access manager.
        m_accessManager.setProxy(proxy);
    }

    void NetworkManager::abortDownloads()
    {
        m_timeoutTimer.stop();

        QMapIterator<QNetworkReply*, QUrl> itr(m_downloadRequests);
        while (itr.hasNext())
        {
            // Tell the reply to abort.
            QNetworkReply* reply = itr.next().key();
            if (reply->isRunning())
            {
                reply->abort();
            }
        }

        QMutexLocker lock(&m_mutex_downloading_image);
        m_downloadRequests.clear();
    }

    int NetworkManager::downloadQueueSize() const
    {
        // Default return value.
        int return_size(0);

        // Return the size of the downloading image queue.
        QMutexLocker lock(&m_mutex_downloading_image);
        return_size += m_downloadRequests.size();

        // Return the size.
        return return_size;
    }

    bool NetworkManager::isDownloading(const QUrl& url) const
    {
        // Return whether we requested url is downloading image queue.
        QMutexLocker lock(&m_mutex_downloading_image);
        return m_downloadRequests.values().contains(url);
    }

    void NetworkManager::downloadImage(const QUrl& url, bool cacheOnly)
    {
        // Keep track of our success.
        bool success(false);

        if (!m_timeoutTimer.isActive())
        {
            m_timeoutTimer.start();
        }

        // Scope this as we later call "downloadQueueSize()" which also locks all download queue mutexes.
        {
            // Gain a lock to protect the downloading image container.
            QMutexLocker lock(&m_mutex_downloading_image);

            // Check this is a new request.
            if (m_downloadRequests.values().contains(url) == false)
            {
                // Generate a new request.
                QNetworkRequest request(url);
                request.setRawHeader("User-Agent", "QMapControl");

                // Using pipelining QNAM can put multiple requests in a single packet
                // (just a suggestion and needs server support).
                request.setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute, true);

                if (m_accessManager.cache() != nullptr)
                {
                    // Prefer fresh tiles from network
                    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferNetwork);
                    // Data obtained should be saved to cache for future uses
                    request.setAttribute(QNetworkRequest::CacheSaveControlAttribute, true);
                }

                // Send the request.
                QNetworkReply* reply = m_accessManager.get(request);

                reply->setProperty("cacheOnly", cacheOnly);
                // Time when this request is considered timeouted
                QDateTime timeout = QDateTime::currentDateTime().addSecs(kReplyTimeout_s);
                reply->setProperty("timeout", timeout);

                // Store the request into the downloading image queue.
                m_downloadRequests[reply] = url;

                // Mark our success.
                success = true;

                // Log success.
#ifdef QMAP_DEBUG
                qDebug() << "Downloading image '" << url << "', queued: " << m_downloadRequests.size();
#endif
            }
        }

        // Was we successful?
        if (success)
        {
            // Emit that we are downloading a new image (with details of the current queue size).
            emit downloadingInProgress(downloadQueueSize());
        }
    }

    void NetworkManager::proxyAuthenticationRequired(const QNetworkProxy& proxy, QAuthenticator* authenticator)
    {
        // Log proxy authentication request.
        qDebug() << "Proxy Authentication Required for '" << proxy.hostName() << "' with the authenticator '" << &authenticator << "'";

        // Set the stored username/password values into the authenticator to use.
        authenticator->setUser(m_proxyUserName);
        authenticator->setPassword(m_proxyPassword);
    }

    void NetworkManager::downloadFinished(QNetworkReply* reply)
    {
        QNetworkReply::NetworkError error = reply->error();
        bool hasReply = false;

        {
            QMutexLocker lock(&m_mutex_downloading_image);
            hasReply = m_downloadRequests.contains(reply);
            if (hasReply)
            {
                // Remove it from the downloading image queue.
                m_downloadRequests.remove(reply);
            } else {
                qWarning() << "Unexpected reply for: " << reply->url();
            }
        }

        // Did the reply return no errors...
        if (error != QNetworkReply::NoError)
        {
#ifdef QMAP_DEBUG
            // Log error
            qDebug() << "Failed to download '" << reply->url() << "' with error '" << reply->errorString() << "'";
#endif            
            // Report failed requests for tracking progress but leave out
            // cancelations (user-triggered or retries of timeouts)
            if (error != QNetworkReply::OperationCanceledError) {
                emit imageDownloadFailed(reply->url(), error);
            }
        }
        else if (hasReply)
        {
#ifdef QMAP_DEBUG
            qDebug() << "Downloaded image " << reply->url() << ", payload size: " << reply->size();
#endif
            bool cacheOnly = reply->property("cacheOnly").toBool();
            if (cacheOnly)
            {
                emit imageCached(reply->url());
            }
            else
            {
                // Emit that we have downloaded an image.
                QImageReader image_reader(reply);
                QPixmap pixmap= QPixmap::fromImageReader(&image_reader);

                if (pixmap.isNull()) {
                    qWarning() << "Pixmap is empty for " << reply->url() << ", reply size: " << reply->size();
                }

                emit imageDownloaded(reply->url(), pixmap);
            }
        }

        // Check if the current download queue is empty.
        if (downloadQueueSize() == 0)
        {
            // Emit that all queued downloads have finished.
            emit downloadingFinished();
        }

        // Free the reply now to keep the memory usage with a lot of downloads in check
        // (or it would be freed only when QNAM is deleted, easily GB of mem with 10000s tiles downloaded)
        reply->deleteLater();
    }

    void NetworkManager::setCache(QAbstractNetworkCache* cache)
    {       
        m_accessManager.setCache(cache);
    }

    void NetworkManager::checkReplyTimeouts()
    {        
        QMapIterator<QNetworkReply*, QUrl> itr(m_downloadRequests);
        const QDateTime currentTime = QDateTime::currentDateTime();

        while (itr.hasNext())
        {
            itr.next();
            QNetworkReply *reply = itr.key();
            const QUrl url = itr.value();
            const QDateTime timeout = reply->property("timeout").toDateTime();

            if (currentTime > timeout)
            {
                qWarning() << "Request timeout: " << url.toString();

                if (reply->isRunning())
                {
                    const bool cacheOnly = reply->property("cacheOnly").toBool();
                    reply->abort();

                    // Retry the request in a little while
                    QTimer::singleShot(1000, [=]() { downloadImage(url, cacheOnly); });
                }
            }
        }
    }
}
