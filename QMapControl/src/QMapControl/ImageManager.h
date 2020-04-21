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

#pragma once

// Qt includes.
#include <QtCore/QDir>
#include <QtCore/QObject>
#include <QtCore/QList>
#include <QtCore/QUrl>
#include <QtGui/QPixmap>
#include <QtNetwork/QNetworkProxy>
#include <QCache>
#include <QNetworkDiskCache>
#include <QReadWriteLock>
#include <QWaitCondition>

// STL includes.
#include <chrono>
#include <memory>

// Local includes.
#include "qmapcontrol_global.h"
#include "NetworkManager.h"

/*!
 * @author Kai Winter <kaiwinter@gmx.de>
 * @author Chris Stylianou <chris5287@gmail.com>
 */
namespace qmapcontrol
{
    class ITileProvider {
    public:
        virtual bool getTileData(const QUrl& url, QByteArray& data) = 0;
        virtual ~ITileProvider() { }
    };

    class QMAPCONTROL_EXPORT ImageManager : public QObject
    {
        Q_OBJECT
    public:
        enum class CachePolicy {
            AlwaysNetwork,
            PreferNetwork,
            PreferCache,
            AlwaysCache,
        };

    public:
        /*!
         * Get the singleton instance of the Image Manager.
         * @return the singleton instance.
         */
        static ImageManager& get();

        //! Destroys the singleton instance of Image Manager.
        static void destroy();

    public:
        //! Disable copy constructor.
        ImageManager(const ImageManager&) = delete;

        //! Disable copy assignment.
        ImageManager& operator=(const ImageManager&) = delete;

        //! Destructor.
        ~ImageManager() = default;

        /*!
         * Fetch the tile size in pixels.
         * @return the tile size in pixels.
         */
        int tileSizePx() const;

        /*!
         * Set the new tile size (and resets any resources as required).
         * @param tile_size_px The tile size in pixels to set.
         */
        void setTileSizePx(const int tile_size_px);

        /*!
         * Set the network proxy to use.
         * @param proxy The network proxy to use.
         */
        void setProxy(const QNetworkProxy& proxy);

        /*!
         * Aborts all current loading threads.
         * This is useful when changing the zoom-factor.
         */
        void abortLoading();

        /*!
         * Number of images pending in the download queue.
         * @return the number of images pending in the load queue.
         */
        int downloadQueueSize() const;

        /*!
         * If this component doesn't have the image a network query gets started to load it.
         * Fetch the requested image either from an in-memory cache or persistent file cache (if
         * enabled).
         * If the image does not exist, then it is fetched using a network manager and a "loading"
         * placeholder pixmap is returned. Once the image has been downloaded, the image manager
         * will emit "imageReceived" to inform that the image is now ready.
         * @param url The image url to fetch.
         * @return the pixmap of the image.
         */
        QPixmap getImage(const QUrl& url);

        /*!
         * \brief Obtains binary content for a cached url.
         * \param url The image url.
         * \return binary content of the url response. Empty if it is not present locally.
         */
        QByteArray rawImageFromDiskCache(const QUrl& url) const;

        /*!
         * Fetches the requested image using the getImage function, which has been deemed
         * "offscreen" but may be needed soon.
         * @param url The image url to fetch.
         */
        void prefetchImage(const QUrl& url);

        /*!
         * Downloads tile image from network and places it in disk cache (if enabled). Useful
         * for caching some area for later offline use. Cached tiles do not trigger
         * map redraws when received from network nor they are stored in memory cache.
         * \param url
         * \return true if tile is already in cache, false if network request has spawned and
         *         caller should wait for "imageCached" signal.
         */
        bool cacheImageToDisk(const QUrl& url);

        /*!
         * \brief clears all tiles stored in the disk cache.
         */
        void clearDiskCache();

        /*!
         * \brief setLoadingPixmap sets the pixmap displayed when a tile is not yet loaded
         * \param pixmap the pixmap to display
         */
        void setLoadingPixmap(const QPixmap &pixmap);

        /*!
         * \brief setEmptyPixmap sets the pixmap displayed when a tile is empty/out of bounds
         * \param pixmap the pixmap to display
         */
        void setEmptyPixmap(const QPixmap &pixmap);

        /*!
         * Enables the persistent disk cache for tile images (also over application restarts),
         * specifying the directory and max size.
         * @param dir The directory path where the images should be stored.
         * @param capacityMiB Cache capacity in MiB
         * @return whether the persistent cache was enabled.
         */
        bool configureDiskCache(const QDir& dir, int capacityMiB);

        QString getCacheDir() const { return m_diskCache->cacheDirectory(); }
        /*!
         * Sets capacity of memory cache for decoded tile images.
         * @param capacityMiB Max cache capacity in MiB, when full LRU images are deleted
         */
        void setMemoryCacheCapacity(int capacityMiB);

        /*!
         * Sets cache policy (default: AlwaysCache or simply "offline")
         * AlwaysNetwork: always pulls tiles from network, cache is not activated.
         * PreferNetwork: tries to pull latesttile from network, othewise goes to cache
         * PreferCache:   pulls tile from cache before asking over network
         * AlwaysCache:   pulls tiles only from cache, there are no network requests
         * \param policy
         */
        void setCachePolicy(CachePolicy policy);

        /*!
         * Return active caching policy.
         */
        CachePolicy cachePolicy() const { return m_cachePolicy; }

        /*!
         * Custom tile provider can be used to bypass internal tile downloads
         * and provide tiles from user source e.g. database. Memory caching of tiles is
         * still applied with custom provider.
         * \param provider New tile provider or null to reset and use internal
         */
        void setCustomTileProvider(ITileProvider *provider);

    signals:
        /*!
         * Signal emitted to schedule an image resource to be downloaded.
         * @param url The image url to download.
         * @param cacheOnly If true image is only stored to disk cache and not for display.
         */
        void downloadImage(const QUrl& url, bool cacheOnly);

        /*!
         * Signal emitted when a new image has been queued for download.
         * @param count The current size of the download queue.
         */
        void downloadingInProgress(int count);

        /*!
         * Signal emitted when a image download has finished and the download queue is empty.
         */
        void downloadingFinished();

        /*!
         * Signal emitted when an image has been downloaded by the network manager.
         * @param url The url that the image was downloaded from.
         */
        void imageUpdated(const QUrl& url);

        /*!
         * Emited when some image (reuqested by cacheImageToDisk()) has been cached.
         */
        void imageCached();
        /*!
          * Emitted when image download fails for reasons other than cancellation.
          */
        void imageDownloadFailed();

    private slots:
        /*!
         * Slot to handle an image that has been downloaded.
         * @param url The url that the image was downloaded from.
         * @param pixmap The image.
         */
        void handleImageDownloaded(const QUrl& url, const QPixmap& pixmap);

        void handleImageCached(const QUrl& url);

    private:
        //! Constructor.
        /*!
         * This construct a Image Manager.
         * @param tile_size_px The tile size in pixels.
         * @param parent QObject parent ownership.
         */
        ImageManager(const int tile_size_px, QObject* parent = nullptr);
        /*!
         * Create a loading pixmap for use.
         */
        void setupPlaceholderPixmaps();

        /*!
         * Generate a md5 hex for the given url.
         * @param url The url to generate a md5 hex for.
         * @return the md5 hex of the url.
         */
        QByteArray hashTileUrl(const QUrl& url) const;

        void insertTileToMemoryCache(const QUrl& url, const QPixmap& pixmap);
        bool findTileInMemoryCache(const QUrl& url, QPixmap& pixmap) const;

        QPixmap getImageInternal(const QUrl& url);

        QPixmap getImageFromDevice(const QUrl& url, QIODevice* device);

    private:
        /// The tile size in pixels.
        int m_tile_size_px;

        /// Network manager.
        NetworkManager m_networkManager;

        /// Memory cache for decoded tile images
        QCache<QByteArray, QPixmap> m_memoryCache;

        /// Lock for accessing memory tile cache
        mutable QReadWriteLock m_tileCacheLock;

        /// Local disk cache for tile image files
        QNetworkDiskCache* m_diskCache;

        /// Active cache policy
        CachePolicy m_cachePolicy;

        /// Placeholder pixmap for tile being downloaded.
        QPixmap m_pixmapLoading;

        /// Placeholder pixmap for empty tiles (e.g. out of bounds of offline map)
        QPixmap m_pixmapEmpty;

        /// A set of image urls being prefetched.
        QSet<QUrl> m_prefetchUrls;

        /// Custom tile provider
        ITileProvider *m_tileProvider;

        mutable QMutex m_tileProviderLock;
    };
}
