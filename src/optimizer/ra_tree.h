#ifndef ra_tree
#define ra_tree

#include "relational_algebra.h"
#include <set>

class RaTree {
    public:
        RaTree(Ra__Node* _root, std::vector<Ra__Node*> _ctes);
        ~RaTree();

        /// Root node of main relational algebra tree
        Ra__Node* root;

        /// Relational algebra trees of Common Table Expressions
        std::vector<Ra__Node*> ctes;

        /**
         * optimizes the RaTree
         */
        void optimize();

    private:

        /**
         * Find and decorrelate all simply correlated exists in tree.
         * Simply correlated: subquery is correlated through a single equi predicate
         */
        void decorrelate_simple_exists();

        /**
         * Find simple correlated exists
         * @param it p_ptr which will be pointed at the selection marker
         * @param marker_type type of the marker to find
         * @return boolean whether subquery marker was found
         */
        void find_subquery_markers(Ra__Node** it, std::vector<std::pair<Ra__Node*, Ra__Node*>>& markers, Ra__Join__JoinType marker_type);

        /**
         * Find join node with specific marker
         * @param it p_ptr which will be pointed at the join node
         * @param marker the marker to find
         * @param is_left_marker true, if the marker is the left marker in join node
         * @return boolean whether subquery marker was found
         */
        void find_joins_by_markers(Ra__Node** it, std::vector<std::pair<Ra__Node*, Ra__Node*>>& markers);

        void simple_exists_to_in(std::pair<Ra__Node*, Ra__Node*>& exists);

        /**
         * gets relations and alias in immediate from clause of subquery
         */
        void get_relations_aliases(Ra__Node** it, std::set<std::pair<std::string,std::string>>& relations_aliases);

        /**
         * finds first selection in tree
         * @return true if found
         */
        bool get_selection(Ra__Node** it);

        bool get_selection_parent(Ra__Node** it);
};

#endif