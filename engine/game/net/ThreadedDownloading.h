#ifndef _THREADED_TCP_DOWNLOAD_H_
#define _THREADED_TCP_DOWNLOAD_H_

#include "game/net/URLStructInfo.h"
#include "game/helpers/CallbackEvent.h"

namespace ThreadedDownloading
{
	typedef U32 DownloadID;
	class QueuedDownload;

	static DownloadID INVALID_DOWNLOAD_ID = 0xFFFFFFFF;

	//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

	// Initialize this subsystem.
	void Init();

	// Draw debug stuff on-screen.
	void DrawDebug();

	// Download from a URL. Returns the download ID.
	DownloadID Download(const char* url, const char* savePath);

	// Cancel a queued download.
	bool Cancel(DownloadID dId);

	// Get a download object.
	QueuedDownload* GetQueueEntry(DownloadID dId);

	// Shutdown the threaded downloading system
	void Shutdown();

	//-------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

	class QueuedDownload
	{
	public: // Static stuff
		static DownloadID highestDownloadID;
		static QueuedDownload** queue;
		static U32 queueLength;
		static U32 queueSize;

	public: // Linking
		static QueuedDownload* first;
		QueuedDownload* next;
		QueuedDownload* previous;

	protected: // Variables
		UrlStructInfo mURL;
		DownloadID mId;
		char* mSavePath;
		void* mCallbackUserData;
		U32 mHashCode;

	private: // Status variables
		bool mInQueue;
		bool mValid;

	public: // Events
		CallbackEvent onDownloadStart;    // onDownloadStart   ( int id, int iContentLength )
		CallbackEvent onDownloadProgress; // onDownloadProgress( int iLength, int iContentLength, int iTransferRate )
		CallbackEvent onDownloadComplete; // onDownloadComplete( string sFileName )
		CallbackEvent onDownloadFailed;   // onDownloadFailed  ( string sErrorString )

	public: // Initializers
		QueuedDownload();
		~QueuedDownload();

	private: // Private methods
		void GenerateHash();

	public: // Methods
		bool Queue();
		bool Cancel();
		bool Pause();
		bool Resume();

	public: // Sets' & Gets'
		const char* GetSavePath();
		UrlStructInfo* GetURL();

		bool SetSavePath(const char* path);
		bool SetURL(const char* URL);

		bool IsDownloading();
		bool IsInQueue();
		bool IsPaused();
		bool IsValid();

		U32 GetDownloadID();
		U32 GetHashCode();
	};
}

#endif