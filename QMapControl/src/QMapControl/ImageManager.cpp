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

#include "ImageManager.h"

// Qt includes.
#include <QDateTime>
#include <QtCore/QCryptographicHash>
#include <QtCore/QDateTime>
#include <QtGui/QPainter>

// Local includes.
#include "Projection.h"

namespace qmapcontrol
{
    const int kDefaultTileSizePx = 256;
    const int kDefaultPixmapCacheSizeMiB = 30;

    namespace
    {
        /// Singleton instance of Image Manager.
        std::unique_ptr<ImageManager> m_instance = nullptr;
    }

    ImageManager& ImageManager::get()
    {
        // Does the singleton instance exist?
        if(m_instance == nullptr)
        {
            // Create a default instance
            m_instance.reset(new ImageManager(kDefaultTileSizePx));
        }

        // Return the reference to the instance object.
        return *(m_instance.get());
    }

    void ImageManager::destory()
    {
        // Ensure the singleton instance is destroyed.
        m_instance.reset(nullptr);
    }

    ImageManager::ImageManager(const int& tile_size_px, QObject* parent)
        : QObject(parent),
          m_tile_size_px(tile_size_px),
          m_diskCache(new QNetworkDiskCache(this)),
          m_pixmap_loading()
    {        
        setMemoryCacheCapacity(kDefaultPixmapCacheSizeMiB);

        // Setup a loading pixmap.
        setupLoadingPixmap();

        // Connect signal/slot for image downloads.
        QObject::connect(this, &ImageManager::downloadImage, &m_networkManager, &NetworkManager::downloadImage);
        QObject::connect(&m_networkManager, &NetworkManager::imageDownloaded, this, &ImageManager::imageDownloaded);
        QObject::connect(&m_networkManager, &NetworkManager::downloadingInProgress, this, &ImageManager::downloadingInProgress);
        QObject::connect(&m_networkManager, &NetworkManager::downloadingFinished, this, &ImageManager::downloadingFinished);
    }

    int ImageManager::tileSizePx() const
    {
        // Return the tiles size in pixels.
        return m_tile_size_px;
    }

    void ImageManager::setTileSizePx(const int& tile_size_px)
    {
        // Set the new tile size.
        m_tile_size_px = tile_size_px;

        // Create a new loading pixmap.
        setupLoadingPixmap();
    }

    void ImageManager::setProxy(const QNetworkProxy& proxy)
    {
        // Set the proxy on the network manager.
        m_networkManager.setProxy(proxy);
    }

    bool ImageManager::enableDiskCache(const QDir& dir, int capacityMiB)
    {
        // Ensure that the path exists (still returns true when path already exists.
        bool success = dir.mkpath(dir.absolutePath());

        // If the path does exist, enable disk cache.
        if (success)
        {
            m_diskCache->setCacheDirectory(dir.absolutePath());
            m_diskCache->setMaximumCacheSize(capacityMiB * 1024 * 1024);

            m_networkManager.setCache(m_diskCache);
        }
        else
        {
            m_networkManager.setCache(nullptr);
            qDebug() << "Unable to create directory for persistent cache '" << dir.absolutePath() << "'";
        }

        // Return success.
        return success;
    }

    void ImageManager::abortLoading()
    {
        // Abort any remaing network manager downloads.
        m_networkManager.abortDownloads();
    }

    int ImageManager::loadQueueSize() const
    {
        // Return the network manager downloading queue size.
        return m_networkManager.downloadQueueSize();
    }

    QPixmap ImageManager::getImage(const QUrl& url)
    {
        QPixmap return_pixmap;

        // Is the image already been downloaded by the network manager?
        if (!m_networkManager.isDownloading(url))
        {          
            if (findTileInMemoryCache(url, return_pixmap))
            {
                Q_ASSERT(!return_pixmap.isNull());
                // Image found in memory cache, use it
                return return_pixmap;
            }
            else
            {
                // Emit that we need to download the image using the network manager.
                emit downloadImage(url);
            }
        }

        // Image bot found, return "loading" image
        return m_pixmap_loading;
    }

    QPixmap ImageManager::prefetchImage(const QUrl& url)
    {
        // Add the url to the prefetch list.
        m_prefetch_urls.append(url);

        // Return the image for the url.
        return getImage(url);
    }

    void ImageManager::setLoadingPixmap(const QPixmap &pixmap)
    {
        m_pixmap_loading = pixmap;
    }

    void ImageManager::imageDownloaded(const QUrl& url, const QPixmap& pixmap)
    {
#ifdef QMAP_DEBUG
        qDebug() << "ImageManager::imageDownloaded '" << url << "'";
#endif

        // Add it to the pixmap cache
        insertTileToMemoryCache(url, pixmap);

        // Is this a prefetch request?
        if (m_prefetch_urls.contains(url))
        {
            // Remove the url from the prefetch list.
            m_prefetch_urls.removeAt(m_prefetch_urls.indexOf(url));
        }
        else
        {
            // Let the world know we have received an updated image.
            emit imageUpdated(url);
        }
    }

    void ImageManager::setupLoadingPixmap()
    {
        // Create a new pixmap.
        m_pixmap_loading = QPixmap(m_tile_size_px, m_tile_size_px);

        // Make is transparent.
        m_pixmap_loading.fill(Qt::transparent);

        // Add a pattern.
        QPainter painter(&m_pixmap_loading);
        QBrush brush(Qt::lightGray, Qt::Dense5Pattern);
        painter.fillRect(m_pixmap_loading.rect(), brush);

        // Add "LOADING..." text.
        painter.setPen(Qt::black);
        painter.drawText(m_pixmap_loading.rect(), Qt::AlignCenter, "LOADING...");
    }

    QByteArray ImageManager::hashTileUrl(const QUrl& url) const
    {
        // Return the md5 hex value of the given url at a specific projection and tile size.
        return QCryptographicHash::hash((url.toString() + QString::number(projection::get().epsg()) + QString::number(m_tile_size_px)).toUtf8(), QCryptographicHash::Md5).toHex();
    }

    void ImageManager::setMemoryCacheCapacity(int capacityMiB)
    {
        m_memoryCache.setMaxCost(capacityMiB * 1024 * 1024);
    }

    class QPixmapCacheEntry : public QPixmap
    {
    public:
        QPixmapCacheEntry(const QPixmap &pixmap) : QPixmap(pixmap) { }
        ~QPixmapCacheEntry() { }
    };

    void ImageManager::insertTileToMemoryCache(const QUrl& url, const QPixmap& pixmap)
    {
        if (!pixmap.isNull()) {
            int cost = pixmap.width() * pixmap.height() * pixmap.depth() / 8;
            m_memoryCache.insert(hashTileUrl(url), new QPixmapCacheEntry(pixmap), cost);
        }

        qDebug() << "ImageManager: pixmap cache -> total size KiB: " << m_memoryCache.totalCost() / 1024
                 << ", now inserted: " << url.toString();
    }

    bool ImageManager::findTileInMemoryCache(const QUrl& url, QPixmap& pixmap) const
    {
        QPixmap *entry = m_memoryCache.object(hashTileUrl(url));
        if (entry != nullptr) {
            pixmap = *entry;
            return true;
        }

        return false;
    }
}
