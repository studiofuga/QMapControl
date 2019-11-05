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
#include <QCryptographicHash>
#include <QDateTime>
#include <QPainter>
#include <QImageReader>

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

    void ImageManager::destroy()
    {
        // Ensure the singleton instance is destroyed.
        m_instance.reset(nullptr);
    }

    ImageManager::ImageManager(const int& tile_size_px, QObject* parent)
        : QObject(parent),
          m_tile_size_px(tile_size_px),
          m_diskCache(nullptr),
          m_offlineMode(false),
          m_pixmapLoading()
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
            if (m_diskCache == nullptr) {
                m_diskCache = new QNetworkDiskCache(this);
            }
            m_diskCache->setCacheDirectory(dir.absolutePath());
            m_diskCache->setMaximumCacheSize(capacityMiB * 1024 * 1024);

            m_networkManager.setCache(m_diskCache);
        }
        else
        {
            m_networkManager.setCache(nullptr);
            qWarning() << "Unable to create directory for persistent cache '" << dir.absolutePath() << "'";
        }

        // Return success.
        return success;
    }

    void ImageManager::abortLoading()
    {
        // Abort any remaing network manager downloads.
        m_networkManager.abortDownloads();

        m_prefetchUrls.clear();
        m_cacheUrls.clear();
    }

    int ImageManager::loadQueueSize() const
    {
        // Return the network manager downloading queue size.
        return m_networkManager.downloadQueueSize();
    }

    QPixmap ImageManager::getImage(const QUrl& url)
    {
        return getImageInternal(url, false, false);
    }

    QPixmap ImageManager::getImageInternal(const QUrl& url, bool bypassMemChache, bool bypassDiskCache)
    {
        QPixmap return_pixmap;

        if (!bypassMemChache && findTileInMemoryCache(url, return_pixmap))
        {
            Q_ASSERT(!return_pixmap.isNull());
            // Image found in memory cache, use it
            return return_pixmap;
        }

        if (!bypassDiskCache && (m_diskCache != nullptr) && offlineMode())
        {
            // In offline mode check manually if tile is in disk cache.
            // Outside offline mode if cache is set it is used by NetworkManager.
            auto data = m_diskCache->data(url);

            if (data != nullptr) {
                QImageReader image_reader(data);
                return_pixmap = QPixmap::fromImageReader(&image_reader);

                insertTileToMemoryCache(url, return_pixmap);
                if (m_prefetchUrls.contains(url))
                {
                    m_prefetchUrls.remove(url);
                }

                return return_pixmap;
             }
        }

        if (offlineMode()) {
            // In offline mode just look in the caches, no downloads
            return m_pixmapLoading;
        }

        // Is the image already being downloaded by the network manager?
        if (!m_networkManager.isDownloading(url))
        {
            // Emit that we need to download the image using the network manager.
            emit downloadImage(url);
        }

        // Image bot found, return "loading" image
        return m_pixmapLoading;
    }

    void ImageManager::prefetchImage(const QUrl& url)
    {
        QPixmap pixmap;

        // Only if image is not already available
        if (!findTileInMemoryCache(url, pixmap)) {
            // Add the url to the prefetch list.
            m_prefetchUrls.insert(url);
            // Request the image
            (void)getImageInternal(url, true, false);
        }
    }

    void ImageManager::setLoadingPixmap(const QPixmap &pixmap)
    {
        m_pixmapLoading = pixmap;
    }

    void ImageManager::imageDownloaded(const QUrl& url, const QPixmap& pixmap)
    {
#ifdef QMAP_DEBUG
        qDebug() << "ImageManager::imageDownloaded '" << url << "'";
#endif
        bool addToMemCache = true;

        // Is this a prefetch request?
        if (m_prefetchUrls.contains(url))
        {
            // Remove the url from the prefetch list.
            m_prefetchUrls.remove(url);
        }
        else if (m_cacheUrls.contains(url))
        {
            addToMemCache = false;
            m_cacheUrls.remove(url);
            emit imageCached();
        }
        else
        {
            // Let the world know we have received an updated image.
            emit imageUpdated(url);
        }

        if (addToMemCache) {
            // Add it to the pixmap cache
            insertTileToMemoryCache(url, pixmap);
        }
    }

    void ImageManager::setupLoadingPixmap()
    {
        // Create a new pixmap.
        m_pixmapLoading = QPixmap(m_tile_size_px, m_tile_size_px);

        // Make is transparent.
        m_pixmapLoading.fill(Qt::transparent);

        // Add a pattern.
        QPainter painter(&m_pixmapLoading);
        QBrush brush(Qt::lightGray, Qt::Dense5Pattern);
        painter.fillRect(m_pixmapLoading.rect(), brush);

        // Add "LOADING..." text.
        painter.setPen(Qt::black);
        painter.drawText(m_pixmapLoading.rect(), Qt::AlignCenter, "LOADING...");
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
    };

    void ImageManager::insertTileToMemoryCache(const QUrl& url, const QPixmap& pixmap)
    {
        if (!pixmap.isNull()) {
            int cost = pixmap.width() * pixmap.height() * pixmap.depth() / 8;
            m_memoryCache.insert(hashTileUrl(url), new QPixmapCacheEntry(pixmap), cost);
        }

#ifdef QMAP_DEBUG
        qDebug() << "ImageManager: pixmap cache -> total size KiB: " << m_memoryCache.totalCost() / 1024
                 << ", now inserted: " << url.toString();
#endif
    }

    bool ImageManager::findTileInMemoryCache(const QUrl& url, QPixmap& pixmap) const
    {
        QPixmap *entry = m_memoryCache.object(hashTileUrl(url));
        if (entry != nullptr) {
            pixmap = *entry;

#ifdef QMAP_DEBUG
        qDebug() << "ImageManager: found in pixmap cache: " << url.toString();
#endif

            return true;
        }

        return false;
    }

    void ImageManager::cacheImageToDisk(const QUrl& url)
    {
        Q_ASSERT(!m_offlineMode);

        m_cacheUrls.insert(url);
        // Bypass all caches, go directly to netwotk download
        (void)getImageInternal(url, true, true);
    }

    void ImageManager::setOfflineMode(bool enabled)
    {
        m_offlineMode = enabled;
    }

}

