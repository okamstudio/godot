using System.Collections.Generic;

namespace Godot;

/// <summary>
/// Helper Class which will, when Applied to a Node makes it possible to return all children and grandchildren
/// in a Level-Order. Example:
///                 X
///                /|\
///               1 2 3
///              /|   |
///             4 5   6
///              /|
///             7 8
/// </summary>
public class LevelOrderSearch
{
    private readonly Node root;
    private bool hasChildren;

    public LevelOrderSearch(Node root)
    {
        this.root = root;
        hasChildren = true;
    }

    public IEnumerable<T> Iterate<T>() where T : Node
    {
        for (var level = 0; hasChildren; level++)
        {
            hasChildren = false;
            foreach (var child in ReturnLevel<T>(root, level))
            {
                yield return child;
            }
        }
    }

    protected IEnumerable<T> ReturnLevel<T>(Node currentNode, int level) where T : Node
    {
        if (level == 0)
        {
            hasChildren |= currentNode.GetChildCount() > 0;
            if (currentNode is T node)
                yield return node;
        }
        else
        {
            for (var childIdx = 0; childIdx < currentNode.GetChildCount(); childIdx++)
            {
                currentNode.GetChild(childIdx);
            }

            foreach (var node in currentNode.GetChildren())
            {
                foreach (var child in ReturnLevel<T>(node, level - 1))
                {
                    yield return child;
                }
            }
        }
    }
}
