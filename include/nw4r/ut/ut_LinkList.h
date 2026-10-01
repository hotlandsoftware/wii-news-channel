#ifndef NW4R_UT_LINK_LIST_H
#define NW4R_UT_LINK_LIST_H

#include <types.h>

namespace nw4r {
namespace ut {

struct LinkListNode {
    LinkListNode* mNext; // at 0x0
    LinkListNode* mPrev; // at 0x4
};

namespace detail {

class LinkListImpl {
public:
    ~LinkListImpl();

    class Iterator {
    public:
        Iterator() : mPointer(NULL) {}
        explicit Iterator(LinkListNode* node) : mPointer(node) {}

        Iterator& operator++() {
            mPointer = mPointer->mNext;
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

protected:
    u32 mSize;          // at 0x0
    LinkListNode mNode; // at 0x4
};

} // namespace detail

template <typename T, int Ofs> class LinkList : public detail::LinkListImpl {
public:
    class Iterator {
    public:
        Iterator() : mIterator() {}
        explicit Iterator(detail::LinkListImpl::Iterator it) : mIterator(it) {}

        Iterator& operator++() {
            ++mIterator;
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

    Iterator GetBeginIter() { return Iterator(detail::LinkListImpl::GetBeginIter()); }
    Iterator GetEndIter() { return Iterator(detail::LinkListImpl::GetEndIter()); }

    static T* GetPointerFromNode(LinkListNode* node) {
        return reinterpret_cast<T*>(reinterpret_cast<u8*>(node) - Ofs);
    }
};

} // namespace ut
} // namespace nw4r

#endif
