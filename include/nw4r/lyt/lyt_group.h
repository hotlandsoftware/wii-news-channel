#ifndef NW4R_LYT_GROUP_H
#define NW4R_LYT_GROUP_H

#include <types.h>
#include <stddef.h>
#include <nw4r/lyt/lyt_pane.h>
#include <nw4r/lyt/lyt_resources.h>
#include <nw4r/ut/ut_LinkList.h>

namespace nw4r {
namespace lyt {

namespace detail {

struct PaneLink {
    ut::LinkListNode mLink; // at 0x0
    Pane* mTarget;          // at 0x8
};

} // namespace detail

typedef ut::LinkList<detail::PaneLink, offsetof(detail::PaneLink, mLink)> PaneLinkList;

class Group {
public:
    Group();
    Group(const res::Group* pResGroup, Pane* pRootPane);
    virtual ~Group(); // at 0x08

    const char* GetName() const { return mName; }
    bool IsUserAllocated() const { return mbUserAllocated; }
    PaneLinkList& GetPaneList() { return mPaneLinkList; }

    void Init();
    void AppendPane(Pane* pPane);

    ut::LinkListNode mLink; // at 0x04

protected:
    PaneLinkList mPaneLinkList; // at 0x0C
    char mName[16];             // at 0x18
    bool mbUserAllocated;       // at 0x28
    u8 mPadding[3];             // at 0x29
};

typedef ut::LinkList<Group, offsetof(Group, mLink)> GroupList;

class GroupContainer {
public:
    GroupContainer() {}
    ~GroupContainer();

    GroupList& GetGroupList() { return mGroupList; }

    void AppendGroup(Group* pGroup);
    Group* FindGroupByName(const char* findName);

protected:
    GroupList mGroupList; // at 0x0
};

} // namespace lyt
} // namespace nw4r

#endif
