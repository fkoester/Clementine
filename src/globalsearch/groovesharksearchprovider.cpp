#include "groovesharksearchprovider.h"

#include <QIcon>

#include "core/logging.h"
#include "covers/albumcoverloader.h"
#include "internet/groovesharkservice.h"
#include "playlist/songmimedata.h"

GroovesharkSearchProvider::GroovesharkSearchProvider(QObject* parent)
    : service_(NULL) {
}

void GroovesharkSearchProvider::Init(GrooveSharkService* service) {
  service_ = service;
  SearchProvider::Init("GrooveShark", "grooveshark",
                       QIcon(":providers/grooveshark.png"), true, false);
  connect(service_, SIGNAL(SimpleSearchResults(int, SongList)),
          SLOT(SearchDone(int, SongList)));

  cover_loader_ = new BackgroundThreadImplementation<AlbumCoverLoader, AlbumCoverLoader>(this);
  cover_loader_->Start(true);
  cover_loader_->Worker()->SetDesiredHeight(kArtHeight);
  cover_loader_->Worker()->SetPadOutputImage(true);
  cover_loader_->Worker()->SetScaleOutputImage(true);

  connect(cover_loader_->Worker().get(),
          SIGNAL(ImageLoaded(quint64, QImage)),
          SLOT(AlbumArtLoaded(quint64, QImage)));
}

void GroovesharkSearchProvider::SearchAsync(int id, const QString& query) {
  const int service_id = service_->SimpleSearch(query);
  pending_searches_[service_id] = id;

  qLog(Debug) << "Searching grooveshark for:" << query;
}

void GroovesharkSearchProvider::SearchDone(int id, SongList songs) {
  qLog(Debug) << Q_FUNC_INFO;

  // Map back to the original id.
  const int global_search_id = pending_searches_.take(id);

  ResultList ret;
  foreach (const Song& song, songs) {
    Result result(this);
    result.type_ = Result::Type_Track;
    result.metadata_ = song;
    result.match_quality_ = Result::Quality_AtStart;

    ret << result;
  }

  qLog(Debug) << "Found:" << ret.size() << "songs from grooveshark";

  emit ResultsAvailable(global_search_id, ret);
  emit SearchFinished(global_search_id);
}

void GroovesharkSearchProvider::LoadArtAsync(int id, const Result& result) {
  quint64 loader_id = cover_loader_->Worker()->LoadImageAsync(result.metadata_);
  cover_loader_tasks_[loader_id] = id;
}

void GroovesharkSearchProvider::AlbumArtLoaded(quint64 id, const QImage& image) {
  if (!cover_loader_tasks_.contains(id)) {
    return;
  }
  int original_id = cover_loader_tasks_.take(id);
  emit ArtLoaded(original_id, image);
}

void GroovesharkSearchProvider::LoadTracksAsync(int id, const Result& result) {
  SongList ret;

  switch (result.type_) {
    case Result::Type_Track:
      ret << result.metadata_;
      break;

    default:
      // TODO: Implement albums in Grooveshark global search.
      Q_ASSERT(0);
  }

  SortSongs(&ret);

  SongMimeData* mime_data = new SongMimeData;
  mime_data->songs = ret;

  emit TracksLoaded(id, mime_data);
}
