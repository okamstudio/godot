/*
 * Copyright © 2022  Google, Inc.
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * Google Author(s): Garret Rieger
 */

#include "../hb-set.hh"
#include "../hb-priority-queue.hh"
#include "../hb-serialize.hh"

#ifndef GRAPH_GRAPH_HH
#define GRAPH_GRAPH_HH

namespace graph {

/**
 * Represents a serialized table in the form of a graph.
 * Provides methods for modifying and reordering the graph.
 */
struct graph_t
{
  struct vertex_t
  {
    hb_serialize_context_t::object_t obj;
    int64_t distance = 0 ;
    unsigned space = 0 ;
    unsigned start = 0;
    unsigned end = 0;
    unsigned priority = 0;
    private:
    unsigned incoming_edges_ = 0;
    unsigned single_parent = (unsigned) -1;
    hb_hashmap_t<unsigned, unsigned> parents;
    public:

    auto parents_iter () const HB_AUTO_RETURN
    (
      hb_concat (
	hb_iter (&single_parent, single_parent != (unsigned) -1),
	parents.keys_ref ()
      )
    )

    bool in_error () const
    {
      return parents.in_error ();
    }

    bool link_positions_valid (unsigned num_objects, bool removed_nil)
    {
      hb_set_t assigned_bytes;
      for (const auto& l : obj.real_links)
      {
        if (l.objidx >= num_objects
            || (removed_nil && !l.objidx))
        {
          DEBUG_MSG (SUBSET_REPACK, nullptr,
                     "Invalid graph. Invalid object index.");
          return false;
        }

        unsigned start = l.position;
        unsigned end = start + l.width - 1;

        if (unlikely (l.width < 2 || l.width > 4))
        {
          DEBUG_MSG (SUBSET_REPACK, nullptr,
                     "Invalid graph. Invalid link width.");
          return false;
        }

        if (unlikely (end >= table_size ()))
        {
          DEBUG_MSG (SUBSET_REPACK, nullptr,
                     "Invalid graph. Link position is out of bounds.");
          return false;
        }

        if (unlikely (assigned_bytes.intersects (start, end)))
        {
          DEBUG_MSG (SUBSET_REPACK, nullptr,
                     "Invalid graph. Found offsets whose positions overlap.");
          return false;
        }

        assigned_bytes.add_range (start, end);
      }

      return !assigned_bytes.in_error ();
    }

    void normalize ()
    {
      obj.real_links.qsort ();
      for (auto& l : obj.real_links)
      {
        for (unsigned i = 0; i < l.width; i++)
        {
          obj.head[l.position + i] = 0;
        }
      }
    }

    bool equals (const vertex_t& other,
                 const graph_t& graph,
                 const graph_t& other_graph,
                 unsigned depth) const
    {
      if (!(as_bytes () == other.as_bytes ()))
      {
        DEBUG_MSG (SUBSET_REPACK, nullptr,
                   "vertex [%lu] bytes != [%lu] bytes, depth = %u",
                   (unsigned long) table_size (),
                   (unsigned long) other.table_size (),
                   depth);

        auto a = as_bytes ();
        auto b = other.as_bytes ();
        while (a || b)
        {
          DEBUG_MSG (SUBSET_REPACK, nullptr,
                     "  0x%x %s 0x%x", (unsigned) *a, (*a == *b) ? "==" : "!=", (unsigned) *b);
          a++;
          b++;
        }
        return false;
      }

      return links_equal (obj.real_links, other.obj.real_links, graph, other_graph, depth);
    }

    hb_bytes_t as_bytes () const
    {
      return hb_bytes_t (obj.head, table_size ());
    }

    friend void swap (vertex_t& a, vertex_t& b)
    {
      hb_swap (a.obj, b.obj);
      hb_swap (a.distance, b.distance);
      hb_swap (a.space, b.space);
      hb_swap (a.single_parent, b.single_parent);
      hb_swap (a.parents, b.parents);
      hb_swap (a.incoming_edges_, b.incoming_edges_);
      hb_swap (a.start, b.start);
      hb_swap (a.end, b.end);
      hb_swap (a.priority, b.priority);
    }

    hb_hashmap_t<unsigned, unsigned>
    position_to_index_map () const
    {
      hb_hashmap_t<unsigned, unsigned> result;

      result.alloc (obj.real_links.length);
      for (const auto& l : obj.real_links) {
        result.set (l.position, l.objidx);
      }

      return result;
    }

    bool is_shared () const
    {
      return parents.get_population () > 1;
    }

    unsigned incoming_edges () const
    {
      if (HB_DEBUG_SUBSET_REPACK)
       {
	assert (incoming_edges_ == (single_parent != (unsigned) -1) +
		(parents.values_ref () | hb_reduce (hb_add, 0)));
       }
      return incoming_edges_;
    }

    unsigned incoming_edges_from_parent (unsigned parent_index) const {
      if (single_parent != (unsigned) -1) {
        return single_parent == parent_index ? 1 : 0;
      }

      unsigned* count;
      return  parents.has(parent_index, &count) ? *count : 0;
    }

    void reset_parents ()
    {
      incoming_edges_ = 0;
      single_parent = (unsigned) -1;
      parents.reset ();
    }

    void add_parent (unsigned parent_index)
    {
      assert (parent_index != (unsigned) -1);
      if (incoming_edges_ == 0)
      {
	single_parent = parent_index;
	incoming_edges_ = 1;
	return;
      }
      else if (single_parent != (unsigned) -1)
      {
        assert (incoming_edges_ == 1);
	if (!parents.set (single_parent, 1))
	  return;
	single_parent = (unsigned) -1;
      }

      unsigned *v;
      if (parents.has (parent_index, &v))
      {
        (*v)++;
	incoming_edges_++;
      }
      else if (parents.set (parent_index, 1))
	incoming_edges_++;
    }

    void remove_parent (unsigned parent_index)
    {
      if (parent_index == single_parent)
      {
	single_parent = (unsigned) -1;
	incoming_edges_--;
	return;
      }

      unsigned *v;
      if (parents.has (parent_index, &v))
      {
	incoming_edges_--;
	if (*v > 1)
	  (*v)--;
	else
	  parents.del (parent_index);

	if (incoming_edges_ == 1)
	{
	  single_parent = *parents.keys ();
	  parents.reset ();
	}
      }
    }

    void remove_real_link (unsigned child_index, const void* offset)
    {
      unsigned count = obj.real_links.length;
      for (unsigned i = 0; i < count; i++)
      {
        auto& link = obj.real_links.arrayZ[i];
        if (link.objidx != child_index)
          continue;

        if ((obj.head + link.position) != offset)
          continue;

        obj.real_links.remove_unordered (i);
        return;
      }
    }

    bool remap_parents (const hb_vector_t<unsigned>& id_map)
    {
      if (single_parent != (unsigned) -1)
      {
        assert (single_parent < id_map.length);
	single_parent = id_map[single_parent];
	return true;
      }

      hb_hashmap_t<unsigned, unsigned> new_parents;
      new_parents.alloc (parents.get_population ());
      for (auto _ : parents)
      {
	assert (_.first < id_map.length);
	assert (!new_parents.has (id_map[_.first]));
	new_parents.set (id_map[_.first], _.second);
      }

      if (parents.in_error() || new_parents.in_error ())
        return false;

      parents = std::move (new_parents);
      return true;
    }

    void remap_parent (unsigned old_index, unsigned new_index)
    {
      if (single_parent != (unsigned) -1)
      {
        if (single_parent == old_index)
	  single_parent = new_index;
        return;
      }

      const unsigned *pv;
      if (parents.has (old_index, &pv))
      {
        unsigned v = *pv;
	if (!parents.set (new_index, v))
          incoming_edges_ -= v;
	parents.del (old_index);

        if (incoming_edges_ == 1)
	{
	  single_parent = *parents.keys ();
	  parents.reset ();
	}
      }
    }

    bool is_leaf () const
    {
      return !obj.real_links.length && !obj.virtual_links.length;
    }

    bool raise_priority ()
    {
      if (has_max_priority ()) return false;
      priority++;
      return true;
    }

    bool give_max_priority ()
    {
      bool result = false;
      while (!has_max_priority()) {
        result = true;
        priority++;
      }
      return result;
    }

    bool has_max_priority () const {
      return priority >= 3;
    }

    size_t table_size () const {
      return obj.tail - obj.head;
    }

    int64_t modified_distance (unsigned order) const
    {
      // TODO(garretrieger): once priority is high enough, should try
      // setting distance = 0 which will force to sort immediately after
      // it's parent where possible.

      int64_t modified_distance =
          hb_clamp (distance + distance_modifier (), (int64_t) 0, 0x7FFFFFFFFFF);
      if (has_max_priority ()) {
        modified_distance = 0;
      }
      return (modified_distance << 18) | (0x003FFFF & order);
    }

    int64_t distance_modifier () const
    {
      if (!priority) return 0;
      int64_t table_size = obj.tail - obj.head;

      if (priority == 1)
        return -table_size / 2;

      return -table_size;
    }

   private:
    bool links_equal (const hb_vector_t<hb_serialize_context_t::object_t::link_t>& this_links,
                      const hb_vector_t<hb_serialize_context_t::object_t::link_t>& other_links,
                      const graph_t& graph,
                      const graph_t& other_graph,
                      unsigned depth) const
    {
      auto a = this_links.iter ();
      auto b = other_links.iter ();

      while (a && b)
      {
        const auto& link_a = *a;
        const auto& link_b = *b;

        if (link_a.width != link_b.width ||
            link_a.is_signed != link_b.is_signed ||
            link_a.whence != link_b.whence ||
            link_a.position != link_b.position ||
            link_a.bias != link_b.bias)
          return false;

        if (!graph.vertices_[link_a.objidx].equals (
                other_graph.vertices_[link_b.objidx], graph, other_graph, depth + 1))
          return false;

        a++;
        b++;
      }

      if (bool (a) != bool (b))
        return false;

      return true;
    }
  };

  template <typename T>
  struct vertex_and_table_t
  {
    vertex_and_table_t () : index (0), vertex (nullptr), table (nullptr)
    {}

    unsigned index;
    vertex_t* vertex;
    T* table;

    operator bool () {
       return table && vertex;
    }
  };

  /*
   * A topological sorting of an object graph. Ordered
   * in reverse serialization order (first object in the
   * serialization is at the end of the list). This matches
   * the 'packed' object stack used internally in the
   * serializer
   */
  template<typename T>
  graph_t (const T& objects)
      : parents_invalid (true),
        distance_invalid (true),
        positions_invalid (true),
        successful (true),
        buffers ()
  {
    num_roots_for_space_.push (1);
    bool removed_nil = false;
    vertices_.alloc (objects.length);
    vertices_scratch_.alloc (objects.length);
    unsigned count = objects.length;
    for (unsigned i = 0; i < count; i++)
    {
      // If this graph came from a serialization buffer object 0 is the
      // nil object. We don't need it for our purposes here so drop it.
      if (i == 0 && !objects.arrayZ[i])
      {
        removed_nil = true;
        continue;
      }

      vertex_t* v = vertices_.push ();
      if (check_success (!vertices_.in_error ()))
        v->obj = *objects.arrayZ[i];

      check_success (v->link_positions_valid (count, removed_nil));

      if (!removed_nil) continue;
      // Fix indices to account for removed nil object.
      for (auto& l : v->obj.all_links_writer ()) {
        l.objidx--;
      }
    }
  }

  ~graph_t ()
  {
    for (char* b : buffers)
      hb_free (b);
  }

  bool operator== (const graph_t& other) const
  {
    return root ().equals (other.root (), *this, other, 0);
  }

  void print () const {
    for (int i = vertices_.length - 1; i >= 0; i--)
    {
      const auto& v = vertices_[i];
      printf("%d: %u [", i, (unsigned int)v.table_size());
      for (const auto &l : v.obj.real_links) {
        printf("%u, ", l.objidx);
      }
      printf("]\n");
    }
  }

  // Sorts links of all objects in a consistent manner and zeroes all offsets.
  void normalize ()
  {
    for (auto& v : vertices_.writer ())
      v.normalize ();
  }

  bool in_error () const
  {
    return !successful ||
        vertices_.in_error () ||
        num_roots_for_space_.in_error ();
  }

  const vertex_t& root () const
  {
    return vertices_[root_idx ()];
  }

  unsigned root_idx () const
  {
    // Object graphs are in reverse order, the first object is at the end
    // of the vector. Since the graph is topologically sorted it's safe to
    // assume the first object has no incoming edges.
    return vertices_.length - 1;
  }

  const hb_serialize_context_t::object_t& object (unsigned i) const
  {
    return vertices_[i].obj;
  }

  bool add_buffer (char* buffer)
  {
    buffers.push (buffer);
    return !buffers.in_error ();
  }

  /*
   * Adds a 16 bit link from parent_id to child_id
   */
  template<typename T>
  void add_link (T* offset,
                 unsigned parent_id,
                 unsigned child_id)
  {
    auto& v = vertices_[parent_id];
    auto* link = v.obj.real_links.push ();
    link->width = 2;
    link->objidx = child_id;
    link->position = (char*) offset - (char*) v.obj.head;
    vertices_[child_id].add_parent (parent_id);
  }

  /*
   * Generates a new topological sorting of graph ordered by the shortest
   * distance to each node if positions are marked as invalid.
   */
  void sort_shortest_distance_if_needed ()
  {
    if (!positions_invalid) return;
    sort_shortest_distance ();
  }


  /*
   * Generates a new topological sorting of graph ordered by the shortest
   * distance to each node.
   */
  void sort_shortest_distance ()
  {
    positions_invalid = true;

    if (vertices_.length <= 1) {
      // Graph of 1 or less doesn't need sorting.
      return;
    }

    update_distances ();

    hb_priority_queue_t<int64_t> queue;
    queue.alloc (vertices_.length);
    hb_vector_t<vertex_t> &sorted_graph = vertices_scratch_;
    if (unlikely (!check_success (sorted_graph.resize (vertices_.length)))) return;
    hb_vector_t<unsigned> id_map;
    if (unlikely (!check_success (id_map.resize (vertices_.length)))) return;

    hb_vector_t<unsigned> removed_edges;
    if (unlikely (!check_success (removed_edges.resize (vertices_.length)))) return;
    update_parents ();

    queue.insert (root ().modified_distance (0), root_idx ());
    int new_id = root_idx ();
    unsigned order = 1;
    while (!queue.in_error () && !queue.is_empty ())
    {
      unsigned next_id = queue.pop_minimum().second;

      sorted_graph[new_id] = std::move (vertices_[next_id]);
      const vertex_t& next = sorted_graph[new_id];

      if (unlikely (!check_success(new_id >= 0))) {
        // We are out of ids. Which means we've visited a node more than once.
        // This graph contains a cycle which is not allowed.
        DEBUG_MSG (SUBSET_REPACK, nullptr, "Invalid graph. Contains cycle.");
        return;
      }

      id_map[next_id] = new_id--;

      for (const auto& link : next.obj.all_links ()) {
        removed_edges[link.objidx]++;
        if (!(vertices_[link.objidx].incoming_edges () - removed_edges[link.objidx]))
          // Add the order that the links were encountered to the priority.
          // This ensures that ties between priorities objects are broken in a consistent
          // way. More specifically this is set up so that if a set of objects have the same
          // distance they'll be added to the topological order in the order that they are
          // referenced from the parent object.
          queue.insert (vertices_[link.objidx].modified_distance (order++),
                        link.objidx);
      }
    }

    check_success (!queue.in_error ());
    check_success (!sorted_graph.in_error ());

    check_success (remap_all_obj_indices (id_map, &sorted_graph));
    vertices_ = std::move (sorted_graph);

    if (!check_success (new_id == -1))
      print_orphaned_nodes ();
  }

  /*
   * Finds the set of nodes (placed into roots) that should be assigned unique spaces.
   * More specifically this looks for the top most 24 bit or 32 bit links in the graph.
   * Some special casing is done that is specific to the layout of GSUB/GPOS tables.
   */
  void find_space_roots (hb_set_t& visited, hb_set_t& roots)
  {
    int root_index = (int) root_idx ();
    for (int i = root_index; i >= 0; i--)
    {
      if (visited.has (i)) continue;

      // Only real links can form 32 bit spaces
      for (auto& l : vertices_[i].obj.real_links)
      {
        if (l.is_signed || l.width < 3)
          continue;

        if (i == root_index && l.width == 3)
          // Ignore 24bit links from the root node, this skips past the single 24bit
          // pointer to the lookup list.
          continue;

        if (l.width == 3)
        {
          // A 24bit offset forms a root, unless there is 32bit offsets somewhere
          // in it's subgraph, then those become the roots instead. This is to make sure
          // that extension subtables beneath a 24bit lookup become the spaces instead
          // of the offset to the lookup.
          hb_set_t sub_roots;
          find_32bit_roots (l.objidx, sub_roots);
          if (sub_roots) {
            for (unsigned sub_root_idx : sub_roots) {
              roots.add (sub_root_idx);
              find_subgraph (sub_root_idx, visited);
            }
            continue;
          }
        }

        roots.add (l.objidx);
        find_subgraph (l.objidx, visited);
      }
    }
  }

  template <typename T, typename ...Ts>
  vertex_and_table_t<T> as_table (unsigned parent, const void* offset, Ts... ds)
  {
    return as_table_from_index<T> (index_for_offset (parent, offset), std::forward<Ts>(ds)...);
  }

  template <typename T, typename ...Ts>
  vertex_and_table_t<T> as_mutable_table (unsigned parent, const void* offset, Ts... ds)
  {
    return as_table_from_index<T> (mutable_index_for_offset (parent, offset), std::forward<Ts>(ds)...);
  }

  template <typename T, typename ...Ts>
  vertex_and_table_t<T> as_table_from_index (unsigned index, Ts... ds)
  {
    if (index >= vertices_.length)
      return vertex_and_table_t<T> ();

    vertex_and_table_t<T> r;
    r.vertex = &vertices_[index];
    r.table = (T*) r.vertex->obj.head;
    r.index = index;
    if (!r.table)
      return vertex_and_table_t<T> ();

    if (!r.table->sanitize (*(r.vertex), std::forward<Ts>(ds)...))
      return vertex_and_table_t<T> ();

    return r;
  }

  // Finds the object id of the object pointed to by the offset at 'offset'
  // within object[node_idx].
  unsigned index_for_offset (unsigned node_idx, const void* offset) const
  {
    const auto& node = object (node_idx);
    if (offset < node.head || offset >= node.tail) return -1;

    unsigned count = node.real_links.length;
    for (unsigned i = 0; i < count; i++)
    {
      // Use direct access for increased performance, this is a hot method.
      const auto& link = node.real_links.arrayZ[i];
      if (offset != node.head + link.position)
        continue;
      return link.objidx;
    }

    return -1;
  }

  // Finds the object id of the object pointed to by the offset at 'offset'
  // within object[node_idx]. Ensures that the returned object is safe to mutate.
  // That is, if the original child object is shared by parents other than node_idx
  // it will be duplicated and the duplicate will be returned instead.
  unsigned mutable_index_for_offset (unsigned node_idx, const void* offset)
  {
    unsigned child_idx = index_for_offset (node_idx, offset);
    auto& child = vertices_[child_idx];
    for (unsigned p : child.parents_iter ())
    {
      if (p != node_idx) {
        return duplicate (node_idx, child_idx);
      }
    }

    return child_idx;
  }


  /*
   * Assign unique space numbers to each connected subgraph of 24 bit and/or 32 bit offset(s).
   * Currently, this is implemented specifically tailored to the structure of a GPOS/GSUB
   * (including with 24bit offsets) table.
   */
  bool assign_spaces ()
  {
    update_parents ();

    hb_set_t visited;
    hb_set_t roots;
    find_space_roots (visited, roots);

    // Mark everything not in the subgraphs of the roots as visited. This prevents
    // subgraphs from being connected via nodes not in those subgraphs.
    visited.invert ();

    if (!roots) return false;

    while (roots)
    {
      uint32_t next = HB_SET_VALUE_INVALID;
      if (unlikely (!check_success (!roots.in_error ()))) break;
      if (!roots.next (&next)) break;

      hb_set_t connected_roots;
      find_connected_nodes (next, roots, visited, connected_roots);
      if (unlikely (!check_success (!connected_roots.in_error ()))) break;

      isolate_subgraph (connected_roots);
      if (unlikely (!check_success (!connected_roots.in_error ()))) break;

      unsigned next_space = this->next_space ();
      num_roots_for_space_.push (0);
      for (unsigned root : connected_roots)
      {
        DEBUG_MSG (SUBSET_REPACK, nullptr, "Subgraph %u gets space %u", root, next_space);
        vertices_[root].space = next_space;
        num_roots_for_space_[next_space] = num_roots_for_space_[next_space] + 1;
        distance_invalid = true;
        positions_invalid = true;
      }

      // TODO(grieger): special case for GSUB/GPOS use extension promotions to move 16 bit space
      //                into the 32 bit space as needed, instead of using isolation.
    }



    return true;
  }

  /*
   * Isolates the subgraph of nodes reachable from root. Any links to nodes in the subgraph
   * that originate from outside of the subgraph will be removed by duplicating the linked to
   * object.
   *
   * Indices stored in roots will be updated if any of the roots are duplicated to new indices.
   */
  bool isolate_subgraph (hb_set_t& roots)
  {
    update_parents ();
    hb_map_t subgraph;

    // incoming edges to root_idx should be all 32 bit in length so we don't need to de-dup these
    // set the subgraph incoming edge count to match all of root_idx's incoming edges
    hb_set_t parents;
    for (unsigned root_idx : roots)
    {
      subgraph.set (root_idx, wide_parents (root_idx, parents));
      find_subgraph (root_idx, subgraph);
    }
    if (subgraph.in_error ())
      return false;

    unsigned original_root_idx = root_idx ();
    hb_map_t index_map;
    bool made_changes = false;
    for (auto entry : subgraph.iter ())
    {
      assert (entry.first < vertices_.length);
      const auto& node = vertices_[entry.first];
      unsigned subgraph_incoming_edges = entry.second;

      if (subgraph_incoming_edges < node.incoming_edges ())
      {
        // Only  de-dup objects with incoming links from outside the subgraph.
        made_changes = true;
        duplicate_subgraph (entry.first, index_map);
      }
    }

    if (in_error ())
      return false;

    if (!made_changes)
      return false;

    if (original_root_idx != root_idx ()
        && parents.has (original_root_idx))
    {
      // If the root idx has changed since parents was determined, update root idx in parents
      parents.add (root_idx ());
      parents.del (original_root_idx);
    }

    auto new_subgraph =
        + subgraph.keys ()
        | hb_map([&] (uint32_t node_idx) {
          const uint32_t *v;
          if (index_map.has (node_idx, &v)) return *v;
          return node_idx;
        })
        ;

    remap_obj_indices (index_map, new_subgraph);
    remap_obj_indices (index_map, parents.iter (), true);

    // Update roots set with new indices as needed.
    for (auto next : roots)
    {
      const uint32_t *v;
      if (index_map.has (next, &v))
      {
        roots.del (next);
        roots.add (*v);
      }
    }

    return true;
  }

  void find_subgraph (unsigned node_idx, hb_map_t& subgraph)
  {
    for (const auto& link : vertices_[node_idx].obj.all_links ())
    {
      hb_codepoint_t *v;
      if (subgraph.has (link.objidx, &v))
      {
        (*v)++;
        continue;
      }
      subgraph.set (link.objidx, 1);
      find_subgraph (link.objidx, subgraph);
    }
  }

  void find_subgraph (unsigned node_idx, hb_set_t& subgraph)
  {
    if (subgraph.has (node_idx)) return;
    subgraph.add (node_idx);
    for (const auto& link : vertices_[node_idx].obj.all_links ())
      find_subgraph (link.objidx, subgraph);
  }

  size_t find_subgraph_size (unsigned node_idx, hb_set_t& subgraph, unsigned max_depth = -1)
  {
    if (subgraph.has (node_idx)) return 0;
    subgraph.add (node_idx);

    const auto& o = vertices_[node_idx].obj;
    size_t size = o.tail - o.head;
    if (max_depth == 0)
      return size;

    for (const auto& link : o.all_links ())
      size += find_subgraph_size (link.objidx, subgraph, max_depth - 1);
    return size;
  }

  /*
   * Finds the topmost children of 32bit offsets in the subgraph starting
   * at node_idx. Found indices are placed into 'found'.
   */
  void find_32bit_roots (unsigned node_idx, hb_set_t& found)
  {
    for (const auto& link : vertices_[node_idx].obj.all_links ())
    {
      if (!link.is_signed && link.width == 4) {
        found.add (link.objidx);
        continue;
      }
      find_32bit_roots (link.objidx, found);
    }
  }

  /*
   * Moves the child of old_parent_idx pointed to by old_offset to a new
   * vertex at the new_offset.
   */
  template<typename O>
  void move_child (unsigned old_parent_idx,
                   const O* old_offset,
                   unsigned new_parent_idx,
                   const O* new_offset)
  {
    distance_invalid = true;
    positions_invalid = true;

    auto& old_v = vertices_[old_parent_idx];
    auto& new_v = vertices_[new_parent_idx];

    unsigned child_id = index_for_offset (old_parent_idx,
                                          old_offset);

    auto* new_link = new_v.obj.real_links.push ();
    new_link->width = O::static_size;
    new_link->objidx = child_id;
    new_link->position = (const char*) new_offset - (const char*) new_v.obj.head;

    auto& child = vertices_[child_id];
    child.add_parent (new_parent_idx);

    old_v.remove_real_link (child_id, old_offset);
    child.remove_parent (old_parent_idx);
  }

  /*
   * duplicates all nodes in the subgraph reachable from node_idx. Does not re-assign
   * links. index_map is updated with mappings from old id to new id. If a duplication has already
   * been performed for a given index, then it will be skipped.
   */
  void duplicate_subgraph (unsigned node_idx, hb_map_t& index_map)
  {
    if (index_map.has (node_idx))
      return;

    unsigned clone_idx = duplicate (node_idx);
    if (!check_success (clone_idx != (unsigned) -1))
      return;

    index_map.set (node_idx, clone_idx);
    for (const auto& l : object (node_idx).all_links ()) {
      duplicate_subgraph (l.objidx, index_map);
    }
  }

  /*
   * Creates a copy of node_idx and returns it's new index.
   */
  unsigned duplicate (unsigned node_idx)
  {
    positions_invalid = true;
    distance_invalid = true;

    auto* clone = vertices_.push ();
    auto& child = vertices_[node_idx];
    if (vertices_.in_error ()) {
      return -1;
    }

    clone->obj.head = child.obj.head;
    clone->obj.tail = child.obj.tail;
    clone->distance = child.distance;
    clone->space = child.space;
    clone->reset_parents ();

    unsigned clone_idx = vertices_.length - 2;
    for (const auto& l : child.obj.real_links)
    {
      clone->obj.real_links.push (l);
      vertices_[l.objidx].add_parent (clone_idx);
    }
    for (const auto& l : child.obj.virtual_links)
    {
      clone->obj.virtual_links.push (l);
      vertices_[l.objidx].add_parent (clone_idx);
    }

    check_success (!clone->obj.real_links.in_error ());
    check_success (!clone->obj.virtual_links.in_error ());

    // The last object is the root of the graph, so swap back the root to the end.
    // The root's obj idx does change, however since it's root nothing else refers to it.
    // all other obj idx's will be unaffected.
    hb_swap (vertices_[vertices_.length - 2], *clone);

    // Since the root moved, update the parents arrays of all children on the root.
    for (const auto& l : root ().obj.all_links ())
      vertices_[l.objidx].remap_parent (root_idx () - 1, root_idx ());

    return clone_idx;
  }

  /*
   * Creates a copy of child and re-assigns the link from
   * parent to the clone. The copy is a shallow copy, objects
   * linked from child are not duplicated.
   *
   * Returns the index of the newly created duplicate.
   *
   * If the child_idx only has incoming edges from parent_idx, this
   * will do nothing and return the original child_idx.
   */
  unsigned duplicate_if_shared (unsigned parent_idx, unsigned child_idx)
  {
    unsigned new_idx = duplicate (parent_idx, child_idx);
    if (new_idx == (unsigned) -1) return child_idx;
    return new_idx;
  }


  /*
   * Creates a copy of child and re-assigns the link from
   * parent to the clone. The copy is a shallow copy, objects
   * linked from child are not duplicated.
   *
   * Returns the index of the newly created duplicate.
   *
   * If the child_idx only has incoming edges from parent_idx,
   * duplication isn't possible and this will return -1.
   */
  unsigned duplicate (unsigned parent_idx, unsigned child_idx)
  {
    update_parents ();

    const auto& child = vertices_[child_idx];
    unsigned links_to_child = child.incoming_edges_from_parent(parent_idx);

    if (child.incoming_edges () <= links_to_child)
    {
      // Can't duplicate this node, doing so would orphan the original one as all remaining links
      // to child are from parent.
      DEBUG_MSG (SUBSET_REPACK, nullptr, "  Not duplicating %u => %u",
                 parent_idx, child_idx);
      return -1;
    }

    DEBUG_MSG (SUBSET_REPACK, nullptr, "  Duplicating %u => %u",
               parent_idx, child_idx);

    unsigned clone_idx = duplicate (child_idx);
    if (clone_idx == (unsigned) -1) return -1;
    // duplicate shifts the root node idx, so if parent_idx was root update it.
    if (parent_idx == clone_idx) parent_idx++;

    auto& parent = vertices_[parent_idx];
    for (auto& l : parent.obj.all_links_writer ())
    {
      if (l.objidx != child_idx)
        continue;

      reassign_link (l, parent_idx, clone_idx);
    }

    return clone_idx;
  }

  /*
   * Creates a copy of child and re-assigns the links from
   * parents to the clone. The copy is a shallow copy, objects
   * linked from child are not duplicated.
   *
   * Returns the index of the newly created duplicate.
   *
   * If the child_idx only has incoming edges from parents,
   * duplication isn't possible or duplication fails and this will
   * return -1.
   */
  unsigned duplicate (const hb_set_t* parents, unsigned child_idx)
  {
    if (parents->is_empty()) {
      return -1;
    }

    update_parents ();

    const auto& child = vertices_[child_idx];
    unsigned links_to_child = 0;
    unsigned last_parent = parents->get_max();
    unsigned first_parent = parents->get_min();
    for (unsigned parent_idx : *parents) {
      links_to_child += child.incoming_edges_from_parent(parent_idx);
    }

    if (child.incoming_edges () <= links_to_child)
    {
      // Can't duplicate this node, doing so would orphan the original one as all remaining links
      // to child are from parent.
      DEBUG_MSG (SUBSET_REPACK, nullptr, "  Not duplicating %u, ..., %u => %u", first_parent, last_parent, child_idx);
      return -1;
    }

    DEBUG_MSG (SUBSET_REPACK, nullptr, "  Duplicating %u, ..., %u => %u", first_parent, last_parent, child_idx);

    unsigned clone_idx = duplicate (child_idx);
    if (clone_idx == (unsigned) -1) return false;

    for (unsigned parent_idx : *parents) {
      // duplicate shifts the root node idx, so if parent_idx was root update it.
      if (parent_idx == clone_idx) parent_idx++;
      auto& parent = vertices_[parent_idx];
      for (auto& l : parent.obj.all_links_writer ())
      {
        if (l.objidx != child_idx)
          continue;

        reassign_link (l, parent_idx, clone_idx);
      }
    }

    return clone_idx;
  }


  /*
   * Adds a new node to the graph, not connected to anything.
   */
  unsigned new_node (char* head, char* tail)
  {
    positions_invalid = true;
    distance_invalid = true;

    auto* clone = vertices_.push ();
    if (vertices_.in_error ()) {
      return -1;
    }

    clone->obj.head = head;
    clone->obj.tail = tail;
    clone->distance = 0;
    clone->space = 0;

    unsigned clone_idx = vertices_.length - 2;

    // The last object is the root of the graph, so swap back the root to the end.
    // The root's obj idx does change, however since it's root nothing else refers to it.
    // all other obj idx's will be unaffected.
    hb_swap (vertices_[vertices_.length - 2], *clone);

    // Since the root moved, update the parents arrays of all children on the root.
    for (const auto& l : root ().obj.all_links ())
      vertices_[l.objidx].remap_parent (root_idx () - 1, root_idx ());

    return clone_idx;
  }

  /*
   * Raises the sorting priority of all children.
   */
  bool raise_childrens_priority (unsigned parent_idx)
  {
    DEBUG_MSG (SUBSET_REPACK, nullptr, "  Raising priority of all children of %u",
               parent_idx);
    // This operation doesn't change ordering until a sort is run, so no need
    // to invalidate positions. It does not change graph structure so no need
    // to update distances or edge counts.
    auto& parent = vertices_[parent_idx].obj;
    bool made_change = false;
    for (auto& l : parent.all_links_writer ())
      made_change |= vertices_[l.objidx].raise_priority ();
    return made_change;
  }

  bool is_fully_connected ()
  {
    update_parents();

    if (root().incoming_edges ())
      // Root cannot have parents.
      return false;

    for (unsigned i = 0; i < root_idx (); i++)
    {
      if (!vertices_[i].incoming_edges ())
        return false;
    }
    return true;
  }

#if 0
  /*
   * Saves the current graph to a packed binary format which the repacker fuzzer takes
   * as a seed.
   */
  void save_fuzzer_seed (hb_tag_t tag) const
  {
    FILE* f = fopen ("./repacker_fuzzer_seed", "w");
    fwrite ((void*) &tag, sizeof (tag), 1, f);

    uint16_t num_objects = vertices_.length;
    fwrite ((void*) &num_objects, sizeof (num_objects), 1, f);

    for (const auto& v : vertices_)
    {
      uint16_t blob_size = v.table_size ();
      fwrite ((void*) &blob_size, sizeof (blob_size), 1, f);
      fwrite ((const void*) v.obj.head, blob_size, 1, f);
    }

    uint16_t link_count = 0;
    for (const auto& v : vertices_)
      link_count += v.obj.real_links.length;

    fwrite ((void*) &link_count, sizeof (link_count), 1, f);

    typedef struct
    {
      uint16_t parent;
      uint16_t child;
      uint16_t position;
      uint8_t width;
    } link_t;

    for (unsigned i = 0; i < vertices_.length; i++)
    {
      for (const auto& l : vertices_[i].obj.real_links)
      {
        link_t link {
          (uint16_t) i, (uint16_t) l.objidx,
          (uint16_t) l.position, (uint8_t) l.width
        };
        fwrite ((void*) &link, sizeof (link), 1, f);
      }
    }

    fclose (f);
  }
#endif

  void print_orphaned_nodes ()
  {
    if (!DEBUG_ENABLED(SUBSET_REPACK)) return;

    DEBUG_MSG (SUBSET_REPACK, nullptr, "Graph is not fully connected.");
    parents_invalid = true;
    update_parents();

    if (root().incoming_edges ()) {
      DEBUG_MSG (SUBSET_REPACK, nullptr, "Root node has incoming edges.");
    }

    for (unsigned i = 0; i < root_idx (); i++)
    {
      const auto& v = vertices_[i];
      if (!v.incoming_edges ())
        DEBUG_MSG (SUBSET_REPACK, nullptr, "Node %u is orphaned.", i);
    }
  }

  unsigned num_roots_for_space (unsigned space) const
  {
    return num_roots_for_space_[space];
  }

  unsigned next_space () const
  {
    return num_roots_for_space_.length;
  }

  void move_to_new_space (const hb_set_t& indices)
  {
    num_roots_for_space_.push (0);
    unsigned new_space = num_roots_for_space_.length - 1;

    for (unsigned index : indices) {
      auto& node = vertices_[index];
      num_roots_for_space_[node.space] = num_roots_for_space_[node.space] - 1;
      num_roots_for_space_[new_space] = num_roots_for_space_[new_space] + 1;
      node.space = new_space;
      distance_invalid = true;
      positions_invalid = true;
    }
  }

  unsigned space_for (unsigned index, unsigned* root = nullptr) const
  {
  loop:
    assert (index < vertices_.length);
    const auto& node = vertices_[index];
    if (node.space)
    {
      if (root != nullptr)
        *root = index;
      return node.space;
    }

    if (!node.incoming_edges ())
    {
      if (root)
        *root = index;
      return 0;
    }

    index = *node.parents_iter ();
    goto loop;
  }

  void err_other_error () { this->successful = false; }

  size_t total_size_in_bytes () const {
    size_t total_size = 0;
    unsigned count = vertices_.length;
    for (unsigned i = 0; i < count; i++) {
      size_t size = vertices_.arrayZ[i].obj.tail - vertices_.arrayZ[i].obj.head;
      total_size += size;
    }
    return total_size;
  }


 private:

  /*
   * Returns the numbers of incoming edges that are 24 or 32 bits wide.
   */
  unsigned wide_parents (unsigned node_idx, hb_set_t& parents) const
  {
    unsigned count = 0;
    for (unsigned p : vertices_[node_idx].parents_iter ())
    {
      // Only real links can be wide
      for (const auto& l : vertices_[p].obj.real_links)
      {
        if (l.objidx == node_idx
            && (l.width == 3 || l.width == 4)
            && !l.is_signed)
        {
          count++;
          parents.add (p);
        }
      }
    }
    return count;
  }

  bool check_success (bool success)
  { return this->successful && (success || ((void) err_other_error (), false)); }

 public:
  /*
   * Creates a map from objid to # of incoming edges.
   */
  void update_parents ()
  {
    if (!parents_invalid) return;

    unsigned count = vertices_.length;

    for (unsigned i = 0; i < count; i++)
      vertices_.arrayZ[i].reset_parents ();

    for (unsigned p = 0; p < count; p++)
    {
      for (auto& l : vertices_.arrayZ[p].obj.all_links ())
        vertices_[l.objidx].add_parent (p);
    }

    for (unsigned i = 0; i < count; i++)
      // parents arrays must be accurate or downstream operations like cycle detection
      // and sorting won't work correctly.
      check_success (!vertices_.arrayZ[i].in_error ());

    parents_invalid = false;
  }

  /*
   * compute the serialized start and end positions for each vertex.
   */
  void update_positions ()
  {
    if (!positions_invalid) return;

    unsigned current_pos = 0;
    for (int i = root_idx (); i >= 0; i--)
    {
      auto& v = vertices_[i];
      v.start = current_pos;
      current_pos += v.obj.tail - v.obj.head;
      v.end = current_pos;
    }

    positions_invalid = false;
  }

  /*
   * Finds the distance to each object in the graph
   * from the initial node.
   */
  void update_distances ()
  {
    if (!distance_invalid) return;

    // Uses Dijkstra's algorithm to find all of the shortest distances.
    // https://en.wikipedia.org/wiki/Dijkstra%27s_algorithm
    //
    // Implementation Note:
    // Since our priority queue doesn't support fast priority decreases
    // we instead just add new entries into the queue when a priority changes.
    // Redundant ones are filtered out later on by the visited set.
    // According to https://www3.cs.stonybrook.edu/~rezaul/papers/TR-07-54.pdf
    // for practical performance this is faster then using a more advanced queue
    // (such as a fibonacci queue) with a fast decrease priority.
    unsigned count = vertices_.length;
    for (unsigned i = 0; i < count; i++)
      vertices_.arrayZ[i].distance = hb_int_max (int64_t);
    vertices_.tail ().distance = 0;

    hb_priority_queue_t<int64_t> queue;
    queue.alloc (count);
    queue.insert (0, vertices_.length - 1);

    hb_vector_t<bool> visited;
    visited.resize (vertices_.length);

    while (!queue.in_error () && !queue.is_empty ())
    {
      unsigned next_idx = queue.pop_minimum ().second;
      if (visited[next_idx]) continue;
      const auto& next = vertices_[next_idx];
      int64_t next_distance = vertices_[next_idx].distance;
      visited[next_idx] = true;

      for (const auto& link : next.obj.all_links ())
      {
        if (visited[link.objidx]) continue;

        const auto& child = vertices_.arrayZ[link.objidx].obj;
        unsigned link_width = link.width ? link.width : 4; // treat virtual offsets as 32 bits wide
        int64_t child_weight = (child.tail - child.head) +
                               ((int64_t) 1 << (link_width * 8)) * (vertices_.arrayZ[link.objidx].space + 1);
        int64_t child_distance = next_distance + child_weight;

        if (child_distance < vertices_.arrayZ[link.objidx].distance)
        {
          vertices_.arrayZ[link.objidx].distance = child_distance;
          queue.insert (child_distance, link.objidx);
        }
      }
    }

    check_success (!queue.in_error ());
    if (!check_success (queue.is_empty ()))
    {
      print_orphaned_nodes ();
      return;
    }

    distance_invalid = false;
  }

 private:
  /*
   * Updates a link in the graph to point to a different object. Corrects the
   * parents vector on the previous and new child nodes.
   */
  void reassign_link (hb_serialize_context_t::object_t::link_t& link,
                      unsigned parent_idx,
                      unsigned new_idx)
  {
    unsigned old_idx = link.objidx;
    link.objidx = new_idx;
    vertices_[old_idx].remove_parent (parent_idx);
    vertices_[new_idx].add_parent (parent_idx);
  }

  /*
   * Updates all objidx's in all links using the provided mapping. Corrects incoming edge counts.
   */
  template<typename Iterator, hb_requires (hb_is_iterator (Iterator))>
  void remap_obj_indices (const hb_map_t& id_map,
                          Iterator subgraph,
                          bool only_wide = false)
  {
    if (!id_map) return;
    for (unsigned i : subgraph)
    {
      for (auto& link : vertices_[i].obj.all_links_writer ())
      {
        const uint32_t *v;
        if (!id_map.has (link.objidx, &v)) continue;
        if (only_wide && !(link.width == 4 && !link.is_signed)) continue;

        reassign_link (link, i, *v);
      }
    }
  }

  /*
   * Updates all objidx's in all links using the provided mapping.
   */
  bool remap_all_obj_indices (const hb_vector_t<unsigned>& id_map,
                              hb_vector_t<vertex_t>* sorted_graph) const
  {
    unsigned count = sorted_graph->length;
    for (unsigned i = 0; i < count; i++)
    {
      if (!(*sorted_graph)[i].remap_parents (id_map))
        return false;
      for (auto& link : sorted_graph->arrayZ[i].obj.all_links_writer ())
      {
        link.objidx = id_map[link.objidx];
      }
    }
    return true;
  }

  /*
   * Finds all nodes in targets that are reachable from start_idx, nodes in visited will be skipped.
   * For this search the graph is treated as being undirected.
   *
   * Connected targets will be added to connected and removed from targets. All visited nodes
   * will be added to visited.
   */
  void find_connected_nodes (unsigned start_idx,
                             hb_set_t& targets,
                             hb_set_t& visited,
                             hb_set_t& connected)
  {
    if (unlikely (!check_success (!visited.in_error ()))) return;
    if (visited.has (start_idx)) return;
    visited.add (start_idx);

    if (targets.has (start_idx))
    {
      targets.del (start_idx);
      connected.add (start_idx);
    }

    const auto& v = vertices_[start_idx];

    // Graph is treated as undirected so search children and parents of start_idx
    for (const auto& l : v.obj.all_links ())
      find_connected_nodes (l.objidx, targets, visited, connected);

    for (unsigned p : v.parents_iter ())
      find_connected_nodes (p, targets, visited, connected);
  }

 public:
  // TODO(garretrieger): make private, will need to move most of offset overflow code into graph.
  hb_vector_t<vertex_t> vertices_;
  hb_vector_t<vertex_t> vertices_scratch_;
 private:
  bool parents_invalid;
  bool distance_invalid;
  bool positions_invalid;
  bool successful;
  hb_vector_t<unsigned> num_roots_for_space_;
  hb_vector_t<char*> buffers;
};

}

#endif  // GRAPH_GRAPH_HH
