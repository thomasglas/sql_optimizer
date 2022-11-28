#ifndef ra_tree
#define ra_tree

#include "relational_algebra.h"
#include <set>

class RaTree {
    public:
        RaTree(Ra__Node* _root, std::vector<Ra__Node*> _ctes, uint64_t _counter);
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
        // to generate unique ids
        uint64_t counter;

        /**
         * Find and decorrelate all correlated exists in tree.
         * Simply correlated: subquery is correlated through a single equi predicate
         */
        void decorrelate_exists();

        /**
         * Transform trivially correlated exists subquery to uncorrelated in subquery
         */
        void decorrelate_exists_trivial(bool is_boolean_predicate, std::tuple<Ra__Node*,Ra__Node*,std::string,size_t> correlating_predicate, Ra__Node__Selection* sel, std::pair<Ra__Node*, Ra__Node*> exists);

        
        void decorrelate_exists_complex(std::vector<std::tuple<Ra__Node*,Ra__Node*,std::string,size_t>> correlating_predicates, std::pair<Ra__Node*, Ra__Node*> exists);

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

        void check_exists_correlation_type(std::pair<Ra__Node*, Ra__Node*> exists);

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

        bool is_trivially_correlated(std::vector<std::tuple<Ra__Node*,Ra__Node*,std::string,size_t>> correlating_predicates);
        
        bool is_simply_correlated(std::vector<std::tuple<Ra__Node*,Ra__Node*,std::string,size_t>> correlating_predicates);

        void extract_predicates(Ra__Node* it, std::vector<std::tuple<Ra__Node*,Ra__Node*,std::string,size_t>>& correlating_predicates, bool& is_boolean_predicate, std::set<std::pair<std::string,std::string>> relations_aliases);
};

#endif