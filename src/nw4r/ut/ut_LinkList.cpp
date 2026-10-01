#include <nw4r/ut/ut_LinkList.h>

namespace nw4r {
namespace ut {
namespace detail {

LinkListImpl::~LinkListImpl() {
    Clear();
}

LinkListImpl::Iterator LinkListImpl::Erase(Iterator it) {
    Iterator clone(it);
    return Erase(it, ++clone);
}

void LinkListImpl::Clear() {
    Erase(GetBeginIter(), GetEndIter());
}

LinkListImpl::Iterator LinkListImpl::Insert(Iterator it, LinkListNode* node) {
    LinkListNode* next = it.mPointer;
    LinkListNode* prev = next->mPrev;

    node->mNext = next;
    node->mPrev = prev;

    next->mPrev = node;
    prev->mNext = node;

    mSize++;

    return Iterator(node);
}

LinkListImpl::Iterator LinkListImpl::Erase(LinkListNode* node) {
    LinkListNode* next = node->mNext;
    LinkListNode* prev = node->mPrev;

    next->mPrev = prev;
    prev->mNext = next;

    mSize--;

    node->mNext = NULL;
    node->mPrev = NULL;

    return Iterator(next);
}

LinkListImpl::Iterator LinkListImpl::Erase(Iterator begin, Iterator end) {
    LinkListNode* it = begin.mPointer;
    LinkListNode* last = end.mPointer;

    while (it != last) {
        LinkListNode* next = it->mNext;
        Erase(it);
        it = next;
    }

    return Iterator(last);
}

} // namespace detail
} // namespace ut
} // namespace nw4r
