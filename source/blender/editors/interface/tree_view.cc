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
 * \ingroup edinterface
 */

#include "DNA_userdef_types.h"

#include "interface_intern.h"

#include "UI_interface.h"

#include "UI_tree_view.hh"

namespace blender::ui {

/* ---------------------------------------------------------------------- */

/**
 * Add a tree to the container. This is the only place where items should be added, it handles
 * important invariants!
 */
uiAbstractTreeViewItem &uiTreeViewItemContainer::add_tree_item(
    std::unique_ptr<uiAbstractTreeViewItem> item)
{
  children_.append(std::move(item));

  /* The first item that will be added to the root sets this. */
  if (root_ == nullptr) {
    root_ = this;
  }

  uiAbstractTreeViewItem &added_item = *children_.last();
  added_item.root_ = root_;
  if (root_ != this) {
    /* Any item that isn't the root can be assumed to the a #uiAbstractTreeViewItem. Not entirely
     * nice to static_cast this, but well... */
    added_item.parent_ = static_cast<uiAbstractTreeViewItem *>(this);
  }

  return added_item;
}

void uiTreeViewItemContainer::foreach_item_recursive(ItemIterFn iter_fn, IterOptions options) const
{
  for (auto &child : children_) {
    iter_fn(*child);
    if (bool(options & IterOptions::SkipCollapsed) && child->is_collapsed()) {
      continue;
    }

    child->foreach_item_recursive(iter_fn, options);
  }
}

/* ---------------------------------------------------------------------- */

void uiAbstractTreeView::foreach_item(ItemIterFn iter_fn, IterOptions options) const
{
  foreach_item_recursive(iter_fn, options);
}

void uiAbstractTreeView::build_layout_from_tree(const uiTreeViewLayoutBuilder &builder)
{
  uiLayout *prev_layout = builder.current_layout();

  uiLayoutColumn(prev_layout, true);

  foreach_item([&builder](uiAbstractTreeViewItem &item) { builder.build_row(item); },
               IterOptions::SkipCollapsed);

  UI_block_layout_set_current(&builder.block(), prev_layout);
}

void uiAbstractTreeView::update_from_old(uiBlock &new_block)
{
  uiBlock *old_block = new_block.oldblock;
  if (!old_block) {
    return;
  }

  uiTreeViewHandle *old_view_handle = ui_block_view_find_matching_in_old_block(
      &new_block, reinterpret_cast<uiTreeViewHandle *>(this));
  if (!old_view_handle) {
    return;
  }

  uiAbstractTreeView &old_view = reinterpret_cast<uiAbstractTreeView &>(*old_view_handle);
  update_children_from_old_recursive(*this, old_view);
}

void uiAbstractTreeView::update_children_from_old_recursive(
    const uiTreeViewItemContainer &new_items, const uiTreeViewItemContainer &old_items)
{
  for (const auto &new_item : new_items.children_) {
    uiAbstractTreeViewItem *matching_old_item = find_matching_child(*new_item, old_items);
    if (!matching_old_item) {
      continue;
    }

    new_item->update_from_old(*matching_old_item);

    /* Recurse into children of the matched item. */
    update_children_from_old_recursive(*new_item, *matching_old_item);
  }
}

uiAbstractTreeViewItem *uiAbstractTreeView::find_matching_child(
    const uiAbstractTreeViewItem &lookup_item, const uiTreeViewItemContainer &items)
{
  for (const auto &iter_item : items.children_) {
    if (lookup_item.label_ == iter_item->label_) {
      /* We have a matching item! */
      return iter_item.get();
    }
  }

  return nullptr;
}

/* ---------------------------------------------------------------------- */

void uiAbstractTreeViewItem::update_from_old(uiAbstractTreeViewItem &old)
{
  is_open_ = old.is_open_;
}

int uiAbstractTreeViewItem::count_parents() const
{
  int i = 0;
  for (uiTreeViewItemContainer *parent = parent_; parent; parent = parent->parent_) {
    i++;
  }
  return i;
}

bool uiAbstractTreeViewItem::is_collapsed() const
{
  return is_collapsible() && !is_open_;
}

void uiAbstractTreeViewItem::toggle_collapsed()
{
  is_open_ = !is_open_;
}

void uiAbstractTreeViewItem::set_collapsed(bool collapsed)
{
  is_open_ = !collapsed;
}

bool uiAbstractTreeViewItem::is_collapsible() const
{
  return !children_.is_empty();
}

/* ---------------------------------------------------------------------- */

uiTreeViewBuilder::uiTreeViewBuilder(uiBlock &block) : block_(block)
{
}

void uiTreeViewBuilder::build_tree_view(uiAbstractTreeView &tree_view)
{
  tree_view.build_tree();
  tree_view.update_from_old(block_);
  tree_view.build_layout_from_tree(uiTreeViewLayoutBuilder(block_));
}

/* ---------------------------------------------------------------------- */

uiTreeViewLayoutBuilder::uiTreeViewLayoutBuilder(uiBlock &block) : block_(block)
{
}

void uiTreeViewLayoutBuilder::build_row(uiAbstractTreeViewItem &item) const
{
  uiLayout *prev_layout = current_layout();
  uiLayout *row = uiLayoutRow(prev_layout, false);

  item.build_row(*row);

  UI_block_layout_set_current(&block(), prev_layout);
}

uiBlock &uiTreeViewLayoutBuilder::block() const
{
  return block_;
}

uiLayout *uiTreeViewLayoutBuilder::current_layout() const
{
  return block().curlayout;
}

/* ---------------------------------------------------------------------- */

uiBasicTreeViewItem::uiBasicTreeViewItem(StringRef label, BIFIconID icon_) : icon(icon_)
{
  label_ = label;
}

static void but_collapsed_toggle_fn(struct bContext *UNUSED(C), void *but_arg1, void *UNUSED(arg2))
{
  uiButTreeRow *tree_row_but = (uiButTreeRow *)but_arg1;
  uiAbstractTreeViewItem &tree_item = reinterpret_cast<uiAbstractTreeViewItem &>(
      *tree_row_but->tree_item);

  tree_item.toggle_collapsed();
}

void uiBasicTreeViewItem::build_row(uiLayout &row)
{
  uiBlock *block = uiLayoutGetBlock(&row);
  tree_row_but_ = (uiButTreeRow *)uiDefIconTextBut(block,
                                                   UI_BTYPE_TREEROW,
                                                   0,
                                                   /* TODO allow icon despite the chevron icon? */
                                                   get_draw_icon(),
                                                   label_.data(),
                                                   0,
                                                   0,
                                                   UI_UNIT_X,
                                                   UI_UNIT_Y,
                                                   nullptr,
                                                   0,
                                                   0,
                                                   0,
                                                   0,
                                                   nullptr);

  tree_row_but_->tree_item = reinterpret_cast<uiTreeViewItemHandle *>(this);
  UI_but_func_set(&tree_row_but_->but, but_collapsed_toggle_fn, tree_row_but_, nullptr);
  UI_but_treerow_indentation_set(&tree_row_but_->but, count_parents());
}

BIFIconID uiBasicTreeViewItem::get_draw_icon() const
{
  if (icon) {
    return icon;
  }

  if (is_collapsible()) {
    return is_collapsed() ? ICON_TRIA_RIGHT : ICON_TRIA_DOWN;
  }

  return ICON_NONE;
}

uiBut *uiBasicTreeViewItem::button()
{
  return &tree_row_but_->but;
}

}  // namespace blender::ui
