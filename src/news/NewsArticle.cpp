#include <news/NewsArticle.h>
#include <news/System.h>
#include <revolution/mem.h>
#include <wchar.h>

// Not yet decompiled: allocators and the JPEG decoder in other files.
extern MEMAllocator gNewsAllocator; // general allocator
extern MEMAllocator gPictureAllocator; // picture allocator
extern s32 gBlinkPhase;

extern "C" {
void* fn_80040A28(size_t size, MEMAllocator* allocator);
void* fn_80040A48(size_t size, MEMAllocator* allocator);
void fn_8004E748(void* decoder);
void fn_8004E754(void* decoder, s32 flags);
NewsTexture* fn_8004E794(void* decoder, const void* data, u32 size, MEMAllocator* allocator);
}

inline void* operator new(size_t size, MEMAllocator* allocator) {
    return fn_80040A28(size, allocator);
}

inline void* operator new[](size_t size, MEMAllocator* allocator) {
    return fn_80040A48(size, allocator);
}

class JPEGDecoder {
public:
    JPEGDecoder() { fn_8004E748(this); }
    ~JPEGDecoder() { fn_8004E754(this, -1); }

    NewsTexture* Decode(const void* data, u32 size, MEMAllocator* allocator) {
        return fn_8004E794(this, data, size, allocator);
    }

private:
    u8 mWork[0x1C38];
};

static const u32 sIconLocal[4][2] = {
    {67, 67},
    {68, 68},
    {69, 67},
    {68, 68},
};

static const u32 sIcon[4][2] = {
    {77, 77},
    {78, 78},
    {79, 77},
    {78, 78},
};

NewsArticle::NewsArticle(NewsHeader* file, NewsEntryRec* entry, u32 topic, u32 index, BOOL isCurrent) {
    mPrevSame = NULL;
    mNextSame = NULL;
    mFile = file;
    mEntry = entry;
    mText = NULL;
    mSource = NULL;
    mLocation = NULL;
    unk1C = 0;
    mHeadlineText = NULL;
    mBody = NULL;
    mSourceName = NULL;
    mCopyright = NULL;
    mLocationName = NULL;
    mHeadline = NULL;
    mShortHeadline = NULL;
    unk3C = NULL;
    unk40 = NULL;
    mTopic = topic;
    mIndex = index;
    mInvalid = FALSE;
    unk58 = 0;
    unk5C = 0;
    mPictureError = false;
    mFlags = isCurrent ? 2 : 0;

    u32 num = file->numArticles;
    if (num != 0) {
        NewsTextBuffer* text = (NewsTextBuffer*)file->At(file->articlesOfs);
        for (u32 i = 0; i < num; i++, text++) {
            if (text->id == mEntry->articleId) {
                mText = text;
                goto found;
            }
        }
    }
    mInvalid = TRUE;
    return;

found:
    mHeadlineText = (wchar_t*)mFile->At(mText->headlineOfs);
    mBody = (wchar_t*)mFile->At(mText->bodyOfs);

    if (mText->sourceIdx < mFile->numSources && mFile->sourcesOfs != 0) {
        mSource = (NewsSourceRec*)mFile->At(mFile->sourcesOfs) + mText->sourceIdx;
        mSourceName = (wchar_t*)mFile->At(mSource->nameOfs);
        mCopyright = (wchar_t*)mFile->At(mSource->copyrightOfs);
    }

    if (mFile->numLocations > mText->locationIdx && mFile->locationsOfs != 0) {
        mLocation = (NewsLocationRec*)mFile->At(mFile->locationsOfs) + mText->locationIdx;
        mLocationName = (wchar_t*)mFile->At(mLocation->nameOfs);
        unk5C = wcslen(mLocationName) + 1;
        unk40 = new (&gNewsAllocator) wchar_t[unk5C];
        if (unk40 == NULL) {
            gAllocFailed = true;
            return;
        }
    }

    unk58 = mText->size / 2 + 1;
    if (mSourceName != NULL) {
        unk58 += mSource->nameSize / 2 + 1;
    }

    mHeadline = new (&gNewsAllocator) wchar_t[unk58];
    if (mHeadline == NULL) {
        gAllocFailed = true;
        return;
    }
    mShortHeadline = new (&gNewsAllocator) wchar_t[unk58];
    if (mShortHeadline == NULL) {
        gAllocFailed = true;
        return;
    }
    unk3C = new (&gNewsAllocator) wchar_t[unk58];
    if (unk3C == NULL) {
        gAllocFailed = true;
        return;
    }

    wcscpy(mHeadline, mHeadlineText);
    if (mSourceName != NULL) {
        wcscat(mHeadline, L" ");
        wcscat(mHeadline, mSourceName);
    }
}

u32 NewsArticle::GetCategoryIcon() const {
    if (mLocation != NULL) {
        return sIconLocal[mFlags][gBlinkPhase];
    }
    return sIcon[mFlags][gBlinkPhase];
}

void NewsArticle::MarkRead() {
    if (mFlags & 1) {
        return;
    }
    mFlags |= 1;

    NewsArticle* article;
    for (article = mPrevSame; article != NULL; article = article->mPrevSame) {
        article->mFlags |= 1;
    }
    for (article = mNextSame; article != NULL; article = article->mNextSame) {
        article->mFlags |= 1;
    }
}

BOOL NewsArticle::LoadPicture() {
    if (mText->pictureIdx != 0xFFFFFFFF && mText->pictureFileId != 0) {
        mPicture = gNewsData->GetPicture(mText);
        mPictureError = mPicture == NULL;
    }
    return !mPictureError;
}

NewsData::NewsData() {
    mCategories = NULL;
    mNumCategories = 0;
    for (s32 i = 0; i < NEWS_FILE_MAX; i++) {
        mFiles[i] = NULL;
        mLogoCache[i] = NULL;
        mPictureCache[i].pics = NULL;
        mLogoCount[i] = 0;
    }
}

NewsData::~NewsData() {}

s32 NewsData::Init(NewsHeader** files, s32 current) {
    Category* topic;
    NewsHeader* file;
    NewsArticle** slot;
    NewsHeader* src;
    u32 i;
    u32 j;
    s32 n;
    NewsTopicRec* topicRec;
    NewsEntryRec* entry;
    NewsArticle** next;
    NewsTextBuffer* text;
    u32 k;
    NewsSourceRec* source;
    s32 idx;
    u32 count;
    BOOL isCurrent;
    NewsArticle** same;
    NewsArticle* article;

    for (n = 0; n < NEWS_FILE_MAX; n++) {
        mFiles[n] = files[n];
    }

    file = mFiles[current];
    if (file->numTopics == 0) {
        return 6;
    }
    if (file->topicsOfs == 0) {
        return 7;
    }

    mHeader = file;
    mNumCategories = file->numTopics;

    // Count the source logos of each file.
    topicRec = (NewsTopicRec*)file->At(file->topicsOfs);
    for (i = 0; i < mNumCategories; i++, topicRec++) {
        entry = (NewsEntryRec*)file->At(topicRec->entriesOfs);
        for (j = 0; j < topicRec->numEntries; j++, entry++) {
            src = GetFile(entry->fileId);
            if (src == NULL) {
                continue;
            }

            text = (NewsTextBuffer*)src->At(src->articlesOfs);
            for (k = 0; k < src->numArticles; k++, text++) {
                if (text->id == entry->articleId) {
                    break;
                }
            }
            if (k >= src->numArticles) {
                continue;
            }

            source = (NewsSourceRec*)src->At(src->sourcesOfs) + text->sourceIdx;
            if (source->noLogo) {
                continue;
            }

            idx = GetFileIndex(src);
            if (idx >= 0) {
                mLogoCount[idx]++;
            }
        }
    }

    mCategories = new (&gNewsAllocator) Category[mNumCategories];
    if (mCategories == NULL) {
        gAllocFailed = true;
        return 3;
    }

    topicRec = (NewsTopicRec*)file->At(file->topicsOfs);
    mEmpty = true;
    for (i = 0; i < mNumCategories; i++, topicRec++) {
        if (topicRec->numEntries != 0) {
            mEmpty = false;
            break;
        }
    }

    topicRec = (NewsTopicRec*)file->At(file->topicsOfs);
    for (i = 0; i < mNumCategories; topicRec++, i++) {
        if (topicRec->numEntries == 0) {
            continue;
        }
        if (topicRec->entriesOfs == 0) {
            return 3;
        }
        entry = (NewsEntryRec*)file->At(topicRec->entriesOfs);
        for (j = 0; j < topicRec->numEntries; j++, entry++) {
            if (GetFile(entry->fileId) == NULL) {
                return 3;
            }
        }
    }

    for (n = 0; n < NEWS_FILE_MAX; n++) {
        src = mFiles[n];
        if (src != NULL && src->numPictures != 0) {
            mPictureCache[n].fileId = src->id;
            mPictureCache[n].pics = new (&gNewsAllocator) NewsPicture*[src->numPictures];
            if (mPictureCache[n].pics != NULL) {
                for (j = 0; j < src->numPictures; j++) {
                    mPictureCache[n].pics[j] = NULL;
                }
            }
        }
    }

    topic = mCategories;
    topicRec = (NewsTopicRec*)file->At(file->topicsOfs);
    for (i = 0; i < mNumCategories; topicRec++, topic++, i++) {
        count = topicRec->numEntries;
        topic->mNumArticles = count;
        if (count != 0) {
            topic->mArticles = new (&gNewsAllocator) NewsArticle*[count];
            if (topic->mArticles == NULL) {
                gAllocFailed = true;
                return 4;
            }
        }
    }

    topic = mCategories;
    topicRec = (NewsTopicRec*)file->At(file->topicsOfs);
    for (i = 0; i < mNumCategories; topicRec++, topic++, i++) {
        topic->mRec = topicRec;
        topic->mName = (wchar_t*)file->At(topicRec->nameOfs);
        if (topicRec->numEntries == 0) {
            continue;
        }
        entry = (NewsEntryRec*)file->At(topicRec->entriesOfs);
        slot = topic->mArticles;
        for (j = 0; j < topicRec->numEntries; j++, slot++, entry++) {
            src = GetFile(entry->fileId);
            isCurrent = src == mHeader;
            *slot = new (&gNewsAllocator) NewsArticle(src, entry, i, j, isCurrent);
            if (*slot == NULL) {
                gAllocFailed = true;
                return 5;
            }
        }
    }

    // Link articles that appear in several topics.
    topic = mCategories;
    for (u32 i = 0; i < mNumCategories; i++, topic++) {
        slot = topic->mArticles;
        for (j = 0; j < topic->mNumArticles; j++, slot++) {
            same = FindArticle((*slot)->mText, i, j + 1);
            if (same != NULL) {
                (*slot)->mNextSame = *same;
                (*same)->mPrevSame = *slot;
            }
        }
    }

    LoadLogos();

    // Load the pictures, newest file first.
    for (n = 0; n < NEWS_FILE_MAX; n++) {
        src = mFiles[current];
        topic = mCategories;
        for (u32 i = 0; i < mNumCategories; i++, topic++) {
            slot = topic->mArticles;
            for (u32 j = 0; j < topic->mRec->numEntries; j++, slot++) {
                if ((*slot)->mFile == src) {
                    (*slot)->LoadPicture();
                }
            }
        }
        if (--current < 0) {
            current = NEWS_FILE_MAX - 1;
        }
    }

    // Move the articles whose picture failed to the end.
    topic = mCategories;
    for (i = 0; i < mNumCategories; i++, topic++) {
        slot = topic->mArticles;
        for (j = 0; j < topic->mRec->numEntries; j++, slot++) {
            article = *slot;
            if (article->mPictureError) {
                next = slot + 1;
                for (k = j + 1; k < topic->mRec->numEntries; k++, next++) {
                    if (!(*next)->mPictureError) {
                        *slot = *next;
                        *next = article;
                        break;
                    }
                }
            }
        }
    }

    // And drop them.
    topic = mCategories;
    for (i = 0; i < mNumCategories; i++, topic++) {
        slot = topic->mArticles;
        for (j = 0; j < topic->mRec->numEntries; j++, slot++) {
            if ((*slot)->mPictureError) {
                topic->mNumArticles = j;
                if (j == 0) {
                    topic->mArticles = NULL;
                }
                break;
            }
        }
    }

    return 0;
}

NewsArticle** NewsData::FindArticle(NewsTextBuffer* text, u32 topicIdx, u32 start) {
    Category* topic = &mCategories[topicIdx];
    NewsArticle** slot = &topic->mArticles[start];
    for (u32 j = start; j < topic->mNumArticles; j++, slot++) {
        if ((*slot)->mText == text) {
            return slot;
        }
    }

    topic++;
    for (u32 i = topicIdx + 1; i < mNumCategories; i++, topic++) {
        slot = topic->mArticles;
        for (u32 j = 0; j < topic->mNumArticles; j++, slot++) {
            if ((*slot)->mText == text) {
                return slot;
            }
        }
    }
    return NULL;
}

NewsPicture* NewsData::GetPicture(NewsTextBuffer* text) {
    u32 fileId = text->pictureFileId;
    u32 idx = text->pictureIdx;
    if (fileId == 0 || idx == 0xFFFFFFFF) {
        return NULL;
    }

    PictureCache* cache = mPictureCache;
    JPEGDecoder decoder;
    for (s32 i = 0; i < NEWS_FILE_MAX; i++, cache++) {
        NewsHeader* file = mFiles[i];
        if (file == NULL || fileId != cache->fileId) {
            continue;
        }

        if (cache->pics != NULL) {
            if (idx >= file->numPictures) {
                return NULL;
            }
            if (cache->pics[idx] != NULL) {
                return cache->pics[idx];
            }
            if (file->picturesOfs != 0 && idx < file->numPictures) {
                NewsPictureRec* rec = (NewsPictureRec*)file->At(file->picturesOfs) + idx;
                void* data = file->At(rec->dataOfs);
                NewsPicture* pic = new (&gPictureAllocator) NewsPicture;
                if (pic != NULL) {
                    pic->texture = decoder.Decode(data, rec->size, &gPictureAllocator);
                    if (pic->texture != NULL) {
                        if (rec->captionOfs != 0) {
                            pic->caption = (wchar_t*)file->At(rec->captionOfs);
                            for (wchar_t* p = (wchar_t*)file->At(rec->captionOfs); *p != 0; p++) {
                                if (*p == 0xA0) {
                                    *p = L' ';
                                }
                            }
                        } else {
                            pic->caption = NULL;
                        }
                        if (rec->creditOfs != 0) {
                            pic->credit = (wchar_t*)file->At(rec->creditOfs);
                        } else {
                            pic->credit = NULL;
                        }
                        cache->pics[idx] = pic;
                        return pic;
                    }
                }
            }
        }
        return NULL;
    }
    return NULL;
}

void NewsData::LoadLogos() {
    NewsSourceRec* source;
    Category* topic;
    NewsArticle** slot;
    LogoCache* cache;
    s32 i;
    s32 j;

    for (i = 0; i < NEWS_FILE_MAX; i++) {
        if (mLogoCount[i] != 0) {
            mLogoCache[i] = new (&gNewsAllocator) LogoCache[mLogoCount[i]];
            if (mLogoCache[i] != NULL) {
                cache = mLogoCache[i];
                for (j = 0; j < mLogoCount[i]; j++, cache++) {
                    cache->ofs = 0;
                    cache->tex = NULL;
                }
            }
        }
    }

    topic = mCategories;
    for (i = 0; i < mNumCategories; i++, topic++) {
        slot = topic->mArticles;
        for (j = 0; j < (u32)topic->mNumArticles; j++, slot++) {
            source = (*slot)->mSource;
            if (!source->noLogo) {
                (*slot)->mSourceLogo = LoadLogo((*slot)->mFile, source->logoOfs, source->logoSize);
            }
        }
    }
}

NewsTexture* NewsData::LoadLogo(NewsHeader* file, u32 ofs, u32 size) {
    if (file == NULL || ofs == 0) {
        return NULL;
    }

    s32 idx = GetFileIndex(file);
    LogoCache* cache = mLogoCache[idx];
    for (s32 i = 0; i < mLogoCount[idx]; i++, cache++) {
        if (cache->ofs == ofs) {
            return cache->tex;
        }
        if (cache->ofs == 0) {
            JPEGDecoder decoder;
            NewsTexture* tex = decoder.Decode(file->At(ofs), size, &gPictureAllocator);
            if (tex != NULL) {
                cache->ofs = ofs;
                cache->tex = tex;
                return cache->tex;
            }
            return NULL;
        }
    }
    return NULL;
}
