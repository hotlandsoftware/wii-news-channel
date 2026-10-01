#ifndef NW4R_UT_LINK_LIST_H
#define NW4R_UT_LINK_LIST_H

#include <types.h>
#include <nw4r/ut/ut_NonCopyable.h>

namespace nw4r {
namespace ut {

struct LinkListNode : private NonCopyable {
    // Added for lyt (Task 15): nodes are zeroed on construction (lyt_pane/group/animation ctors).
    LinkListNode() : mNext(NULL), mPrev(NULL) {}

    LinkListNode* GetNext() const { return mNext; }
    LinkListNode* GetPrev() const { return mPrev; }

    LinkListNode* mNext; // at 0x0
    LinkListNode* mPrev; // at 0x4
};

namespace detail {

class LinkListImpl {
public:
    // Added for lyt (Task 15)
    LinkListImpl() { Initialize_(); }

    ~LinkListImpl();

    class Iterator {
        friend class LinkListImpl;

    public:
        Iterator() : mPointer(NULL) {}
        explicit Iterator(LinkListNode* node) : mPointer(node) {}

        Iterator& operator++() {
            mPointer = mPointer->mNext;
            return *this;
        }

        Iterator& operator--() {
            mPointer = mPointer->mPrev;
            return *this;
        }

        LinkListNode* operator->() const { return mPointer; }

        friend bool operator==(Iterator lhs, Iterator rhs) { return lhs.mPointer == rhs.mPointer; }

    private:
        LinkListNode* mPointer; // at 0x0
    };

    Iterator GetBeginIter() { return Iterator(mNode.mNext); }
    Iterator GetEndIter() { return Iterator(&mNode); }

    u32 GetSize() const { return mSize; }
    bool IsEmpty() const { return mSize == 0; }

    Iterator Insert(Iterator it, LinkListNode* node);
    Iterator Erase(Iterator it);
    Iterator Erase(LinkListNode* node);
    Iterator Erase(Iterator begin, Iterator end);
    void Clear();

    void PopFront() { Erase(GetBeginIter()); }
    void PopBack() { Erase(--GetEndIter()); }

protected:
    static Iterator GetIteratorFromPointer(LinkListNode* node) { return Iterator(node); }

    void Initialize_() {
        mSize = 0;
        mNode.mNext = &mNode;
        mNode.mPrev = &mNode;
    }

    u32 mSize;          // at 0x0
    LinkListNode mNode; // at 0x4
};

} // namespace detail

template <typename T, int Ofs> class LinkList : public detail::LinkListImpl {
public:
    class Iterator {
        friend class LinkList;

    public:
        typedef T TElem;

        Iterator() : mIterator() {}
        explicit Iterator(detail::LinkListImpl::Iterator it) : mIterator(it) {}

        Iterator& operator++() {
            ++mIterator;
            return *this;
        }

        Iterator& operator--() {
            --mIterator;
            return *this;
        }

        Iterator operator++(int) {
            Iterator it = *this;
            ++*this;
            return it;
        }

        T& operator*() const { return *GetPointerFromNode(mIterator.operator->()); }
        T* operator->() const { return GetPointerFromNode(mIterator.operator->()); }

        friend bool operator==(Iterator lhs, Iterator rhs) { return lhs.mIterator == rhs.mIterator; }
        friend bool operator!=(Iterator lhs, Iterator rhs) { return !(lhs == rhs); }

    private:
        detail::LinkListImpl::Iterator mIterator; // at 0x0
    };

    LinkList() {}

    Iterator GetBeginIter() { return Iterator(detail::LinkListImpl::GetBeginIter()); }
    Iterator GetEndIter() { return Iterator(detail::LinkListImpl::GetEndIter()); }

    Iterator Insert(Iterator it, T* elem) {
        return Iterator(detail::LinkListImpl::Insert(it.mIterator, GetNodeFromPointer(elem)));
    }

    Iterator Erase(T* elem) { return Iterator(detail::LinkListImpl::Erase(GetNodeFromPointer(elem))); }
    Iterator Erase(Iterator it) { return Iterator(detail::LinkListImpl::Erase(it.mIterator)); }

    void PushBack(T* elem) { Insert(GetEndIter(), elem); }

    T& GetFront() { return *GetBeginIter(); }
    T& GetBack() { return *--GetEndIter(); }

    static Iterator GetIteratorFromPointer(T* elem) {
        return Iterator(detail::LinkListImpl::GetIteratorFromPointer(GetNodeFromPointer(elem)));
    }

    static LinkListNode* GetNodeFromPointer(T* p) {
        return reinterpret_cast<LinkListNode*>(reinterpret_cast<u8*>(p) + Ofs);
    }

    static T* GetPointerFromNode(LinkListNode* node) {
        return reinterpret_cast<T*>(reinterpret_cast<u8*>(node) - Ofs);
    }
};

} // namespace ut
} // namespace nw4r

#endif
