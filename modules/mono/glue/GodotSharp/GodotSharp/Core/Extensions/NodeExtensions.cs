using System;
using System.Collections.Generic;
using System.Linq;

namespace Godot
{
    public partial class Node
    {
        /// <summary>
        /// Fetches a node. The <see cref="NodePath"/> can be either a relative path (from
        /// the current node) or an absolute path (in the scene tree) to a node. If the path
        /// does not exist, a <see langword="null"/> instance is returned and an error
        /// is logged. Attempts to access methods on the return value will result in an
        /// "Attempt to call &lt;method&gt; on a null instance." error.
        /// Note: Fetching absolute paths only works when the node is inside the scene tree
        /// (see <see cref="IsInsideTree"/>).
        /// </summary>
        /// <example>
        /// Example: Assume your current node is Character and the following tree:
        /// <code>
        /// /root
        /// /root/Character
        /// /root/Character/Sword
        /// /root/Character/Backpack/Dagger
        /// /root/MyGame
        /// /root/Swamp/Alligator
        /// /root/Swamp/Mosquito
        /// /root/Swamp/Goblin
        /// </code>
        /// Possible paths are:
        /// <code>
        /// GetNode("Sword");
        /// GetNode("Backpack/Dagger");
        /// GetNode("../Swamp/Alligator");
        /// GetNode("/root/MyGame");
        /// </code>
        /// </example>
        /// <seealso cref="GetNodeOrNull{T}(NodePath)"/>
        /// <param name="path">The path to the node to fetch.</param>
        /// <exception cref="InvalidCastException">
        /// The fetched node can't be casted to the given type <typeparamref name="T"/>.
        /// </exception>
        /// <typeparam name="T">The type to cast to. Should be a descendant of <see cref="Node"/>.</typeparam>
        /// <returns>
        /// The <see cref="Node"/> at the given <paramref name="path"/>.
        /// </returns>
        public T GetNode<T>(NodePath path) where T : class
        {
            return (T)(object)GetNode(path);
        }

        /// <summary>
        /// Similar to <see cref="GetNode"/>, but does not log an error if <paramref name="path"/>
        /// does not point to a valid <see cref="Node"/>.
        /// </summary>
        /// <example>
        /// Example: Assume your current node is Character and the following tree:
        /// <code>
        /// /root
        /// /root/Character
        /// /root/Character/Sword
        /// /root/Character/Backpack/Dagger
        /// /root/MyGame
        /// /root/Swamp/Alligator
        /// /root/Swamp/Mosquito
        /// /root/Swamp/Goblin
        /// </code>
        /// Possible paths are:
        /// <code>
        /// GetNode("Sword");
        /// GetNode("Backpack/Dagger");
        /// GetNode("../Swamp/Alligator");
        /// GetNode("/root/MyGame");
        /// </code>
        /// </example>
        /// <seealso cref="GetNode{T}(NodePath)"/>
        /// <param name="path">The path to the node to fetch.</param>
        /// <typeparam name="T">The type to cast to. Should be a descendant of <see cref="Node"/>.</typeparam>
        /// <returns>
        /// The <see cref="Node"/> at the given <paramref name="path"/>, or <see langword="null"/> if not found.
        /// </returns>
        public T GetNodeOrNull<T>(NodePath path) where T : class
        {
            return GetNodeOrNull(path) as T;
        }

        /// <summary>
        /// Returns a child node by its index (see <see cref="GetChildCount"/>).
        /// This method is often used for iterating all children of a node.
        /// Negative indices access the children from the last one.
        /// To access a child node via its name, use <see cref="GetNode"/>.
        /// </summary>
        /// <seealso cref="GetChildOrNull{T}(int, bool)"/>
        /// <param name="idx">Child index.</param>
        /// <param name="includeInternal">
        /// If <see langword="false"/>, internal children are skipped (see <c>internal</c>
        /// parameter in <see cref="AddChild(Node, bool, InternalMode)"/>).
        /// </param>
        /// <exception cref="InvalidCastException">
        /// The fetched node can't be casted to the given type <typeparamref name="T"/>.
        /// </exception>
        /// <typeparam name="T">The type to cast to. Should be a descendant of <see cref="Node"/>.</typeparam>
        /// <returns>
        /// The child <see cref="Node"/> at the given index <paramref name="idx"/>.
        /// </returns>
        public T GetChild<T>(int idx, bool includeInternal = false) where T : class
        {
            return (T)(object)GetChild(idx, includeInternal);
        }

        /// <summary>
        /// Returns a child node by its Type. Optionally pass a bool to make it look in children of children etc.
        /// To access a child node via its name, use <see cref="GetNode"/>.
        /// To access a child node via its name, use <see cref="GetChild{T}(int,bool)"/>.
        /// </summary>
        /// <seealso cref="GetChildOrNull{T}(int, bool)"/>
        /// <param name="recursive">If not set will only look through immediate children of this node.
        /// If set it will go all the way to the leaves of the tree.
        /// </param>
        /// <typeparam name="T">The type to cast to. Should be a descendant of <see cref="Node"/>.</typeparam>
        /// <returns>
        /// The first child <see cref="Node"/> of that type to be found or default (probably null) if no child was found./>.
        /// </returns>
        public T GetChild<T>(bool recursive = false) where T : Node
        {
            return GetChildren<T>(recursive).FirstOrDefault();
        }

        /// <summary>
        /// Iterates through all children of this Node and returns all which match the type.
        /// It will use Level-Order-Search which is a breadth first approach. The idea is, that the element,
        /// which you are trying to search for, is an immediate or at most a grandchild of the Node.
        /// </summary>
        /// <param name="recursive">If not set will only look through immediate children of this node.
        /// If set it will go all the way to the leaves of the tree.
        /// </param>
        /// <typeparam name="T">Type which all found children have</typeparam>
        /// <returns></returns>
        public IEnumerable<T> GetChildren<T>(bool recursive = false) where T : Node
        {
            if (recursive)
            {
                var levelOrderSearch = new LevelOrderSearch(this);
                foreach (T node in levelOrderSearch.Iterate<T>())
                {
                    yield return node;
                }
                /*foreach (var child in GetChildren()) // (Depth first approach)
                {
                    foreach (var node in child.GetChildren<T>())
                    {
                        yield return node;
                    }
                }*/
            }
            else
            {
                foreach (T node in GetChildren().OfType<T>())
                {
                    yield return node;
                }
            }
        }

        /// <summary>
        /// Returns a child node by its index (see <see cref="GetChildCount"/>).
        /// This method is often used for iterating all children of a node.
        /// Negative indices access the children from the last one.
        /// To access a child node via its name, use <see cref="GetNode"/>.
        /// </summary>
        /// <seealso cref="GetChild{T}(int, bool)"/>
        /// <param name="idx">Child index.</param>
        /// <param name="includeInternal">
        /// If <see langword="false"/>, internal children are skipped (see <c>internal</c>
        /// parameter in <see cref="AddChild(Node, bool, InternalMode)"/>).
        /// </param>
        /// <typeparam name="T">The type to cast to. Should be a descendant of <see cref="Node"/>.</typeparam>
        /// <returns>
        /// The child <see cref="Node"/> at the given index <paramref name="idx"/>, or <see langword="null"/> if not found.
        /// </returns>
        public T GetChildOrNull<T>(int idx, bool includeInternal = false) where T : class
        {
            int count = GetChildCount(includeInternal);
            return idx >= -count && idx < count ? GetChild(idx, includeInternal) as T : null;
        }

        /// <summary>
        /// The node owner. A node can have any other node as owner (as long as it is
        /// a valid parent, grandparent, etc. ascending in the tree). When saving a
        /// node (using <see cref="PackedScene"/>), all the nodes it owns will be saved
        /// with it. This allows for the creation of complex <see cref="SceneTree"/>s,
        /// with instancing and subinstancing.
        /// </summary>
        /// <seealso cref="GetOwnerOrNull{T}"/>
        /// <exception cref="InvalidCastException">
        /// The fetched node can't be casted to the given type <typeparamref name="T"/>.
        /// </exception>
        /// <typeparam name="T">The type to cast to. Should be a descendant of <see cref="Node"/>.</typeparam>
        /// <returns>
        /// The owner <see cref="Node"/>.
        /// </returns>
        public T GetOwner<T>() where T : class
        {
            return (T)(object)Owner;
        }

        /// <summary>
        /// The node owner. A node can have any other node as owner (as long as it is
        /// a valid parent, grandparent, etc. ascending in the tree). When saving a
        /// node (using <see cref="PackedScene"/>), all the nodes it owns will be saved
        /// with it. This allows for the creation of complex <see cref="SceneTree"/>s,
        /// with instancing and subinstancing.
        /// </summary>
        /// <seealso cref="GetOwner{T}"/>
        /// <typeparam name="T">The type to cast to. Should be a descendant of <see cref="Node"/>.</typeparam>
        /// <returns>
        /// The owner <see cref="Node"/>, or <see langword="null"/> if there is no owner.
        /// </returns>
        public T GetOwnerOrNull<T>() where T : class
        {
            return Owner as T;
        }

        /// <summary>
        /// Returns the parent node of the current node, or a <see langword="null"/> instance
        /// if the node lacks a parent.
        /// </summary>
        /// <seealso cref="GetParentOrNull{T}"/>
        /// <exception cref="InvalidCastException">
        /// The fetched node can't be casted to the given type <typeparamref name="T"/>.
        /// </exception>
        /// <typeparam name="T">The type to cast to. Should be a descendant of <see cref="Node"/>.</typeparam>
        /// <returns>
        /// The parent <see cref="Node"/>.
        /// </returns>
        public T GetParent<T>() where T : class
        {
            return (T)(object)GetParent();
        }

        /// <summary>
        /// Returns the parent node of the current node, or a <see langword="null"/> instance
        /// if the node lacks a parent.
        /// </summary>
        /// <seealso cref="GetParent{T}"/>
        /// <typeparam name="T">The type to cast to. Should be a descendant of <see cref="Node"/>.</typeparam>
        /// <returns>
        /// The parent <see cref="Node"/>, or <see langword="null"/> if the node has no parent.
        /// </returns>
        public T GetParentOrNull<T>() where T : class
        {
            return GetParent() as T;
        }
    }
}
