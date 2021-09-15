/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/** \file
 * \ingroup editorui
 */

#pragma once

#include <memory>
#include <string>

#include "BLI_function_ref.hh"
#include "BLI_vector.hh"

#include "UI_resources.h"

struct PointerRNA;
struct uiBlock;
struct uiBut;
struct uiButTreeRow;
struct uiLayout;

namespace blender::ui {

class uiAbstractTreeView;
class uiAbstractTreeViewItem;

/* ---------------------------------------------------------------------- */
/** \name Tree-View Item Container
 * \{ */

/**
 * Helper base class to expose common child-item data and functionality to both #uiAbstractTreeView
 * and #uiAbstractTreeViewItem.
 *
 * That means this type can be used whenever either a #uiAbstractTreeView or a
 * #uiAbstractTreeViewItem is needed.
 */
class uiTreeViewItemContainer {
  friend class uiAbstractTreeView;
  friend class uiAbstractTreeViewItem;

  /* Private constructor, so only the friends above can create this! */
  uiTreeViewItemContainer() = default;

 protected:
  Vector<std::unique_ptr<uiAbstractTreeViewItem>> children_;
  /** Adding the first item to the root will set this, then it's passed on to all children. */
  uiTreeViewItemContainer *root_ = nullptr;
  /** Pointer back to the owning item. */
  uiAbstractTreeViewItem *parent_ = nullptr;

 public:
  enum class IterOptions {
    None = 0,
    SkipCollapsed = 1 << 0,

    /* Keep ENUM_OPERATORS() below updated! */
  };
  using ItemIterFn = FunctionRef<void(uiAbstractTreeViewItem &)>;

  /**
   * Convenience wrapper taking the arguments needed to construct an item of type \a ItemT. Calls
   * the version just below.
   */
  template<class ItemT, typename... Args> ItemT &add_tree_item(Args &&...args)
  {
    static_assert(std::is_base_of<uiAbstractTreeViewItem, ItemT>::value,
                  "Type must derive from and implement the uiAbstractTreeViewItem interface");

    return dynamic_cast<ItemT &>(
        add_tree_item(std::make_unique<ItemT>(std::forward<Args>(args)...)));
  }

  uiAbstractTreeViewItem &add_tree_item(std::unique_ptr<uiAbstractTreeViewItem> item);

 protected:
  void foreach_item_recursive(ItemIterFn iter_fn, IterOptions options = IterOptions::None) const;
};

ENUM_OPERATORS(uiTreeViewItemContainer::IterOptions,
               uiTreeViewItemContainer::IterOptions::SkipCollapsed);

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Tree-View Builders
 * \{ */

class uiTreeViewBuilder {
  uiBlock &block_;

 public:
  uiTreeViewBuilder(uiBlock &block);

  void build_tree_view(uiAbstractTreeView &tree_view);
};

class uiTreeViewLayoutBuilder {
  uiBlock &block_;

  friend uiTreeViewBuilder;

 public:
  void build_row(uiAbstractTreeViewItem &item) const;
  uiBlock &block() const;
  uiLayout *current_layout() const;

 private:
  /* Created through #uiTreeViewBuilder. */
  uiTreeViewLayoutBuilder(uiBlock &block);
};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Tree-View Base Class
 * \{ */

class uiAbstractTreeView : public uiTreeViewItemContainer {
  friend uiTreeViewBuilder;
  friend uiTreeViewLayoutBuilder;

 public:
  virtual ~uiAbstractTreeView() = default;

  void foreach_item(ItemIterFn iter_fn, IterOptions options = IterOptions::None) const;

 protected:
  virtual void build_tree() = 0;

 private:
  void update_from_old(uiBlock &new_block);
  static void update_children_from_old_recursive(const uiTreeViewItemContainer &new_items,
                                                 const uiTreeViewItemContainer &old_items);
  static uiAbstractTreeViewItem *find_matching_child(const uiAbstractTreeViewItem &lookup_item,
                                                     const uiTreeViewItemContainer &items);
  void build_layout_from_tree(const uiTreeViewLayoutBuilder &builder);
};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Tree-View Item Type
 * \{ */

/* * \brief Abstract base class for defining a customizable tree-view item.
 *
 * The tree-view item defines how to build its data into a tree-row. There are implementations for
 * common layouts, e.g. #uiBasicTreeViewItem.
 * It also stores state information that needs to be persistent over redraws, like the collapsed
 * state.
 */
class uiAbstractTreeViewItem : public uiTreeViewItemContainer {
  friend class uiAbstractTreeView;

  bool is_open_ = false;
  bool is_active_ = false;

 protected:
  /** This label is used for identifying an item (together with its parents labels). */
  std::string label_{};

 public:
  virtual ~uiAbstractTreeViewItem() = default;

  virtual void build_row(uiLayout &row) = 0;

  virtual void onActivate();

  /** Copy persistent state (e.g. is-collapsed flag, selection, etc.) from a matching item of the
   * last redraw to this item. If sub-classes introduce more advanced state they should override
   * this and make update their state accordingly. */
  virtual void update_from_old(uiAbstractTreeViewItem &old);

  const uiAbstractTreeView &get_tree_view() const;
  int count_parents() const;
  void set_active(bool value = true);
  bool is_active() const;
  void toggle_collapsed();
  bool is_collapsed() const;
  void set_collapsed(bool collapsed);
  bool is_collapsible() const;
};

/** \} */

/* ---------------------------------------------------------------------- */
/** \name Predefined Tree-View Item Types
 *
 *  Common, Basic Tree-View Item Types.
 * \{ */

/**
 * The most basic type, just a label with an icon.
 */
class uiBasicTreeViewItem : public uiAbstractTreeViewItem {
 public:
  using ActivateFn = std::function<void(uiBasicTreeViewItem &new_active)>;
  BIFIconID icon;

  uiBasicTreeViewItem(StringRef label,
                      BIFIconID icon = ICON_NONE,
                      ActivateFn activate_fn = nullptr);

  void build_row(uiLayout &row) override;
  void onActivate() override;

 protected:
  /** Created in the #build() function. */
  uiButTreeRow *tree_row_but_ = nullptr;
  /** Called when activating this tree view item. */
  ActivateFn activate_fn_;

  uiBut *button();
  BIFIconID get_draw_icon() const;
};

/** \} */

}  // namespace blender::ui
