#ifndef ra_tree
#define ra_tree

#include "relational_algebra.h"
#include <set>
#include <unordered_map>
#include <map>

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
        void decorrelate_all_exists();
        void decorrelate_arbitrary_queries();

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
        void find_subquery_markers(Ra__Node* it, std::vector<std::pair<Ra__Node*, Ra__Node*>>& markers, Ra__Join__JoinType marker_type);

        /**
         * Find join node with specific marker
         * @param it p_ptr which will be pointed at the join node
         * @param marker the marker to find
         * @param is_left_marker true, if the marker is the left marker in join node
         * @return boolean whether subquery marker was found
         */
        void find_joins_by_markers(Ra__Node* it, std::vector<std::pair<Ra__Node*, Ra__Node*>>& markers);

        void replace_selection_marker(Ra__Node* marker_parent, int child_index, Ra__Node* replacement);

        std::tuple<Ra__Node*,Ra__Node*,std::string> is_correlating_predicate(Ra__Node__Predicate* p, const std::vector<std::pair<std::string,std::string>>& relations_aliases);

        void push_down_dep_join(Ra__Node** dep_join_parent);

        void push_down_predicates();

        std::vector<Ra__Node*> intersect_correlated_attributes(Ra__Node* dep_join);

        bool find_marker_parent(Ra__Node** it, Ra__Node__Where_Subquery_Marker* marker, int& child_index);

        void decorrelate_exists_subquery(std::pair<Ra__Node*, Ra__Node*> exists);
        
        void split_selection_predicates(Ra__Node* predicate, std::vector<std::pair<Ra__Node*,std::vector<std::string>>>& predicates_relations);

        void get_predicate_relations(Ra__Node* predicate, std::vector<std::string>& relations);

        bool find_lowest_child_insert_selection(const std::pair<Ra__Node*,std::vector<std::string>>& predicate_relations, Ra__Node* subtree);

        void add_predicate_to_selection(Ra__Node* predicate, Ra__Node* selection);

        void add_predicate_to_join(Ra__Node* predicate, Ra__Node* join);

        void get_all_selections(Ra__Node* it, std::vector<Ra__Node*>& selections);

        bool find_node_where_relations_defined(Ra__Node** it, const std::vector<std::string>& relations);

        bool get_node_parent(Ra__Node** it, Ra__Node* child_target, int& child_index);

        bool has_relations_defined(Ra__Node* node, const std::vector<std::string>& relations);

        void get_predicate_expression_relations(Ra__Node* predicate_expr, std::vector<std::string>& relations);

        std::string get_relation_from_attribute(Ra__Node* attribute);

        void decorrelate_subquery(std::pair<Ra__Node*, Ra__Node*> subquery);

        /**
         * gets relations and alias in immediate from clause of subquery
         */
        void get_relations_aliases(Ra__Node* it, std::vector<std::pair<std::string,std::string>>& relations_aliases);

        /**
         * finds first selection in tree
         * @return true if found
         */
        bool get_first_selection(Ra__Node** it);

        // bool get_selection_parent(Ra__Node** it);

        bool is_trivially_correlated(std::vector<std::tuple<Ra__Node*,Ra__Node*,std::string,size_t>> correlating_predicates);
        
        void get_subtree_attributes(Ra__Node* it, std::vector<Ra__Node*>& attributes);

        bool is_tpch_attribute(std::string attr_name, std::string relation);

        std::string get_tpch_relation_name(std::string attr_name);

        void get_expression_attributes(Ra__Node* expression, std::vector<Ra__Node*>& attributes);

        void get_predicate_attributes(Ra__Node* predicate, std::vector<Ra__Node*>& attributes);

        bool is_simply_correlated(std::vector<std::tuple<Ra__Node*,Ra__Node*,std::string,size_t>> correlating_predicates);

        void get_correlating_predicates(Ra__Node* it, std::vector<std::tuple<Ra__Node*,Ra__Node*,std::string,size_t>>& correlating_predicates, bool& is_boolean_predicate, std::vector<std::pair<std::string,std::string>> relations_aliases);

        bool can_decouple(Ra__Node* selection, const std::vector<Ra__Node*>& d_args, std::map<std::pair<std::string,std::string>, std::pair<std::string,std::string>>& d_rename_map);

        void rename_attributes(Ra__Node* it, std::map<std::pair<std::string,std::string>, std::pair<std::string,std::string>> rename_map);

        void remove_redundant_predicates(Ra__Node* predicate, Ra__Node* predicate_parent);

        bool has_equivalent_attribute(Ra__Node* attribute, Ra__Node* predicate, std::map<std::pair<std::string,std::string>, std::pair<std::string,std::string>>& d_rename_map);
};

#endif