/** Copyright (C) 2017 Tim Kuipers - Released under terms of the AGPLv3 License */
#ifndef UTILS_SPACE_FILLING_TREE_H
#define UTILS_SPACE_FILLING_TREE_H

#include "../utils/polygon.h"
#include "../utils/AABB.h"
#include "../utils/intpoint.h"
#include "../utils/optional.h"

#include "../utils/SVG.h"

namespace cura
{

/*!
 * TODO
 */
class SpaceFillingTree
{
public:
    class LocationVisitor
    {
    public:
        virtual void visit(Point junction) = 0;
    };
    SpaceFillingTree(
        Point middle,
        coord_t radius,
        int depth)
    : root(getRoot(middle, depth))
    {
        // total width = radius becase 1 + .5 + .25 + ... = 2
        // therefore the initial width = .5 * radius
        coord_t first_offset = radius / 2; // horizontal/vertical distance from the middle to the end of the first line segment
        // construct 1st order children
        for (Direction dir = static_cast<Direction>(0); dir < Direction::DIRECTION_COUNT; dir = static_cast<Direction>(dir + 1))
        {
            root.addChild(dir, first_offset);
        }
        root.prune();
    }

    SpaceFillingTree(SpaceFillingTree&& other)
    : aabb(other.aabb)
    , root(other.root)
    { // move constructor
        for (Node*& child : other.root.children)
        {
            child = nullptr;
        }
    }

    ~SpaceFillingTree()
    {
        for (Node* child : root.children)
        {
            delete child;
        }
    }

    void walk(LocationVisitor& visitor) const
    {
        root.walk(visitor);
    }

    void debugOutput(SVG& out, bool output_dfs_order = false)
    {
        debugCheck();

        out.writePolygon(aabb.toPolygon());
        int root_order = 0;
        root.debugOutput(out, root.middle, output_dfs_order, root_order); // root will draw line from its middle to the same middle
    }

    void debugCheck()
    {
        root.debugCheck();
    }
protected:
    /*!
     * TODO
     */
    enum Direction : unsigned char
    {
        LU = 0,
        RU = 1,
        RD = 2,
        LD = 3,
        DIRECTION_COUNT = 4
    };
    /*!
     * TODO
     */
    class  Node
    {
    public:
        Node* parent; // optional because of root
        int depth;
        Point middle;
        Direction parent_to_here_direction;
        Node* children[4];

        Node(Node* parent, int depth, Point middle, Direction parent_to_here_direction)
        : parent(parent)
        , depth(depth)
        , middle(middle)
        , parent_to_here_direction(parent_to_here_direction)
        {
            for (Node*& child_place : children)
            {
                child_place = nullptr;
            }
        }

        ~Node()
        {
            for (Node* child : children)
            {
                delete child;
            }
        }

        void addChild(Direction direction, coord_t child_offset)
        {
            Point child_middle_offset = Point(child_offset, child_offset);
            if (direction == Direction::LD || direction == Direction::RD)
            { // if down
                child_middle_offset.Y *= -1;
            }
            if (direction == Direction::LD || direction == Direction::LU)
            { // if left
                child_middle_offset.X *= -1;
            }
            const Point child_middle = middle + child_middle_offset;
            Node* new_node;
            if (parent && direction == (parent_to_here_direction + 2) % Direction::DIRECTION_COUNT)
            {
                /*
                 * reorder:
                 *   make
                 * parent-->new_node-->this
                 *   rather than
                 * parent------------->this
                 *         new_node<---'
                 */
                assert(parent);
                new_node = new Node(parent, depth - 1, child_middle, parent_to_here_direction);
                new_node->children[parent_to_here_direction] = this;
                parent->children[parent_to_here_direction] = new_node;
                parent = new_node;
                new_node = new_node;
            }
            else
            {
                new_node = new Node(this, depth - 1, child_middle, direction);
                if (children[direction])
                {
                    /*!
                    * reorder
                    *  from
                    * this------------->child
                    *   make
                    * this-->new_node-->child
                    */
                    new_node->children[direction] = this->children[direction];
                }
                this->children[direction] = new_node;
            }

            if (depth <= 0)
            {
                return;
            }
            for (Direction child_dir = static_cast<Direction>(0); child_dir < Direction::DIRECTION_COUNT; child_dir = static_cast<Direction>(child_dir + 1))
            {
                new_node->addChild(child_dir, child_offset / 2);
            }
        }

        void prune()
        {
            for (Direction child_dir = static_cast<Direction>(0); child_dir < Direction::DIRECTION_COUNT; child_dir = static_cast<Direction>(child_dir + 1))
            {
                Node* child = children[child_dir];
                if (!child)
                {
                    continue;
                }
                Node* right = child->children[(child_dir + 1) % Direction::DIRECTION_COUNT];
                Node* front = child->children[child_dir];
                Node* left = child->children[(child_dir + 3) % Direction::DIRECTION_COUNT];
                if (front && !left && !right)
                {
                    children[child_dir] = front;
                    child->children[child_dir] = nullptr; // so that it won't be deleted
                    delete child;
                }
                children[child_dir]->prune(); // prune new or existing child recursively
            }
        }

        void walk(LocationVisitor& visitor) const
        {
            visitor.visit(middle);
            for (int dir_offset = 0; dir_offset < Direction::DIRECTION_COUNT; dir_offset++)
            {
                int direction = (parent_to_here_direction + dir_offset + 2) % Direction::DIRECTION_COUNT;
                Node* child = children[direction];
                if (child)
                {
                    child->walk(visitor);
                    visitor.visit(middle);
                }
            }
        }

        void debugOutput(SVG& out, Point parent_middle, bool output_dfs_order, int& order_nr, bool output_directions = false)
        {
            out.writeLine(parent_middle, middle);
            if (output_dfs_order)
            {
                out.writeText(middle, std::to_string(order_nr));
            }
            for (int dir_offset = 0; dir_offset < Direction::DIRECTION_COUNT; dir_offset++)
            {
                int direction = (parent_to_here_direction + dir_offset + 2) % Direction::DIRECTION_COUNT;
                Node* child = children[direction];
                if (child)
                {
                    order_nr++;
                    if (output_directions)
                    {
                        out.writeText((middle + child->middle) / 2, std::to_string(direction), SVG::Color::BLUE);
                    }
                    child->debugOutput(out, middle, output_dfs_order, order_nr);
                }
            }
        }

        void debugCheck()
        {
            for (Direction child_dir = static_cast<Direction>(0); child_dir < Direction::DIRECTION_COUNT; child_dir = static_cast<Direction>(child_dir + 1))
            {
                Node* child = children[child_dir];
                if (child)
                {
                    assert(child->children[(child_dir + 2) % Direction::DIRECTION_COUNT] == nullptr);
                }
            }
        }
    };
    AABB aabb;
    Node getRoot(Point middle, int depth)
    {
        Node root(nullptr
            , depth
            , middle
            , Direction::DIRECTION_COUNT); // TODO: what should be the direction?
        return root;
    }
    Node root;
};
} // namespace cura


#endif // UTILS_SPACE_FILLING_TREE_H