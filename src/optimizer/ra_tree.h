#ifndef ra_tree
#define ra_tree

#include "relational_algebra.h"
#include <set>
#include <unordered_map>
#include <map>
#include <queue>

class RaTree {
    public:
        RaTree(std::shared_ptr<Ra__Node> _root, std::vector<std::shared_ptr<Ra__Node>> _ctes, uint64_t _counter);

        /// Root node of main relational algebra tree
        std::shared_ptr<Ra__Node> root;

        /// Relational algebra trees of Common Table Expressions
        std::vector<std::shared_ptr<Ra__Node>> ctes;

        /**
         * optimizes the RaTree
         */
        void optimize();

    private:
        // to generate unique ids
        uint64_t counter;

        // works: (false,true), (false,false), (true,true)
        const bool push_down_correlating_predicates = false;
        const bool decouple = true;
        const bool convert_cp_to_join = false;

        void decorrelate_all_exists_in_subqueries();

        void decorrelate_exists_in_subquery(std::pair<std::shared_ptr<Ra__Node>, std::shared_ptr<Ra__Node>> marker_join);

        /**
         * Find and decorrelate all correlated exists in tree.
         */
        void decorrelate_all_exists();

        /**
         * Decorrelates all basic subqueries (excl. exists, in)
         */
        void general_query_unnesting();

        /**
         * Decorrelate a basic subquery
         * @param markers_joins pair with subquery marker and corresponding join node
         */
        void decorrelate_subquery(std::pair<std::shared_ptr<Ra__Node>, std::shared_ptr<Ra__Node>> markers_joins);
        
        /**
         * Transform trivially correlated exists subquery to uncorrelated in subquery
         * 
         * @param is_boolean_predicate if correlating predicate in subquery is part of a boolean predicate
         * @param correlating_predicate correlating predicate in subquery (left, right, operator, child_index)
         * @param sel selection in subquery containing correlating predicate
         * @param markers_joins pair of subquery marker and corresponding exists join node
         */
        void decorrelate_exists_trivial(bool is_boolean_predicate, std::tuple<std::shared_ptr<Ra__Node>,std::shared_ptr<Ra__Node>,std::string,size_t> correlating_predicate, std::shared_ptr<Ra__Node__Selection> sel, std::pair<std::shared_ptr<Ra__Node>, std::shared_ptr<Ra__Node>> markers_joins);

        /**
         * Transform complex correlated exists subquery to uncorrelated in subquery with left join and null check
         * 
         * @param correlating_predicates vector of correlating predicates in subquery (left, right, operator, child_index)
         * @param markers_joins pair of subquery marker and corresponding exists join node
         */
        void decorrelate_exists_complex(std::vector<std::tuple<std::shared_ptr<Ra__Node>,std::shared_ptr<Ra__Node>,std::string,size_t>> correlating_predicates, std::pair<std::shared_ptr<Ra__Node>, std::shared_ptr<Ra__Node>> markers_joins);

        /**
         * Check if subquery is trivially correlated
         * @param correlating_predicates vector with predicates (left, right, operator, child_index)
         * @return true if trivially correlated
         */
        bool is_trivially_correlated(std::vector<std::tuple<std::shared_ptr<Ra__Node>,std::shared_ptr<Ra__Node>,std::string,size_t>> correlating_predicates);
        
        /**
         * decorrelate an exists subquery
         * 
         * @param markers_joins pair containing subquery marker and corresponding join node of exists subquery
         */
        void decorrelate_exists_subquery(std::pair<std::shared_ptr<Ra__Node>, std::shared_ptr<Ra__Node>> markers_joins);

        /**
         * Recursively find all subquery markers within predicates in subtree
         * 
         * @param it RaTree pointer as starting node of search
         * @param markers vector which is filled with all found subquery markers (pair.second is filled with nullptr, will later be used for corresponding subquery nodes)
         * @param marker_type type of subquery markers to find
         */
        void find_subquery_markers(std::shared_ptr<Ra__Node> it, std::vector<std::pair<std::shared_ptr<Ra__Node>, std::shared_ptr<Ra__Node>>>& markers, std::vector<Ra__Join__JoinType> marker_types);

        /**
         * Recursively find all join nodes corresponding to given subquery markers
         * @param it RaTree pointer as starting node of search
         * @param markers vector of subquery markers to find; pair.second is filled with pointers to found join nodes
         */
        void find_joins_by_markers(std::shared_ptr<Ra__Node> it, std::vector<std::pair<std::shared_ptr<Ra__Node>, std::shared_ptr<Ra__Node>>>& markers);

        /**
         * Replace the subquery marker in a predicate with a new RaNode
         * @param marker_parent parent RaNode of subquery marker
         * @param child_index index which subquery marker has in its parent
         * @param replacement RaNode to replace subquery marker with
         */
        void replace_subquery_marker(std::shared_ptr<Ra__Node> marker_parent, int child_index, std::shared_ptr<Ra__Node> replacement);

        /**
         * Checks whether a predicate is correlating
         * @param p predicate to check
         * @param relations_aliases relations and aliases which are defined in the current subquery
         * @return true if predicate is correlating
         */
        std::tuple<std::shared_ptr<Ra__Node>,std::shared_ptr<Ra__Node>,std::string> is_correlating_predicate(std::shared_ptr<Ra__Node__Predicate> p, const std::vector<std::pair<std::string,std::string>>& relations_aliases);

        /**
         * Get correlating predicates in a predicate
         * @param it pointer to subtree
         * @param correlating_predicates vector to fill with correlating predicates (left, right, operator, child_index)
         * @param is_boolean_predicate true if correlating predicate is part of a boolean predicate
         * @param relations_aliases relation names and aliases defined (attribute correlated, if referencing relation outside of these)
         */
        void get_correlating_predicates(std::shared_ptr<Ra__Node> it, std::vector<std::tuple<std::shared_ptr<Ra__Node>,std::shared_ptr<Ra__Node>,std::string,size_t>>& correlating_predicates, bool& is_boolean_predicate, std::vector<std::pair<std::string,std::string>> relations_aliases);

        /**
         * Push down a dependent join through its right child
         * @param dep_join dependent join to push down
         * @param original_right_projection pointer to original subquery projection (dep join is in subtree of projection)
         * @param push_down_dep_joins_queue if new dependent joins are introduces through the push down, add them to this queue
         */
        void push_down_dep_join(std::shared_ptr<Ra__Node__Join> dep_join, std::shared_ptr<Ra__Node> original_right_projection, std::queue<std::shared_ptr<Ra__Node__Join>>& push_down_dep_joins_queue);

        /**
         * Perform predicate push down on the entire RaTree
         * @param cp_to_join whether selections and cross products should be combined to a join
         */
        void push_down_predicates(bool cp_to_join);

        /**
         * Intersect attributes of d projection and given subtree
         * @param d_projection pointer to "d" projection subtree
         * @param comparsion_subtree pointer to subtree to compare
         * @return vector of attributes containing intersection result
         */
        std::vector<std::shared_ptr<Ra__Node>> intersect_correlated_attributes(std::shared_ptr<Ra__Node> d_projection, std::shared_ptr<Ra__Node> comparsion_subtree);

        /**
         * Find subquery marker in a predicate 
         * @param it pointer the predicate, will point to subquery_marker node if returned true
         * @param marker subquery marker to find
         * @param child_index child index which subquery marker has in parent node
         * @return true if found, else false
         */
        bool find_marker_in_predicate(std::shared_ptr<Ra__Node>& it, std::shared_ptr<Ra__Node__Where_Subquery_Marker> marker, int& child_index);

        /**
         * Find the parent node of a subquery marker
         * @param it pointer to node where to start search, will point to parent of subquery_marker node if returned true
         * @param marker subquery marker, whose parent to find
         * @param child_index child index of subquery marker
         * @return true if parent is found, else false
         */
        bool find_marker_parent(std::shared_ptr<Ra__Node>& it, std::shared_ptr<Ra__Node__Where_Subquery_Marker> marker, int& child_index);
        
        /**
         * Splits predicate into splittable subpredicates (split by "and")
         * @param predicate predicate split
         * @param predicates_relations vector of split subpredicates (pair.second is used to in get_predicate_relations)
         */
        void split_selection_predicates(std::shared_ptr<Ra__Node> predicate, std::vector<std::pair<std::shared_ptr<Ra__Node>,std::vector<std::string>>>& predicates_relations);

        /**
         * add a predicate to a selecction predicate (with "and")
         * @param predicate predicate to add
         * @param selection selection to add predicate to
         */
        void add_predicate_to_selection(std::shared_ptr<Ra__Node> predicate, std::shared_ptr<Ra__Node> selection);

        /**
         * add a predicate to a join predicate (with "and")
         * @param predicate predicate to add
         * @param join join to add predicate to
         */
        void add_predicate_to_join(std::shared_ptr<Ra__Node> predicate, std::shared_ptr<Ra__Node> join);

        /**
         * Get all selection nodes in a subtree
         * @param it pointer to subtree
         * @param selections vector to be filled with all found selection nodes
         */
        void get_all_selections(std::shared_ptr<Ra__Node> it, std::vector<std::shared_ptr<Ra__Node>>& selections);

        /**
         * Find the lowest node where a predicate's referenced relations are defined, and insert the predicate in an selection over the node
         * @param predicate_relations predicate and vector of relations referenced in predicate
         * @param selection pointer to selection where predicate is originally from
         * @param cp_to_join whether predicate should be added to join node instead of selection if possible
         * @return true if a child was found where referenced relations are defined
         */
        bool find_lowest_child_insert_selection(const std::pair<std::shared_ptr<Ra__Node>,std::vector<std::string>>& predicate_relations, std::shared_ptr<Ra__Node> selection, bool cp_to_join);

        /**
         * DFS find node in subtree where all given relations are defined
         * @param it pointer to subtree
         * @param relations vector of relations which need to be defined
         * @return true if a node was found, else false
         */
        bool find_node_where_relations_defined(std::shared_ptr<Ra__Node>& it, const std::vector<std::string>& relations);

        /**
         * Checks whether all relations are defined in subtree of this node
         * @param node pointer to node
         * @param relations vector of relations which need to be defined
         * @return true if relations are defined, else false
         */
        bool has_relations_defined(std::shared_ptr<Ra__Node> node, const std::vector<std::string>& relations);

        /**
         * Find the parent node of a node in a subtree
         * @param it pointer to subtree 
         * @param child_target child node, whose parent to find
         * @param child_index child index which node has in parent node
         * @return true if parent node was found
         */
        bool get_node_parent(std::shared_ptr<Ra__Node>& it, std::shared_ptr<Ra__Node> child_target, int& child_index);
 
        /**
         * Get relations referenced in predicate
         * @param predicate predicate to check
         * @param relations vector to fill with referenced relations
         */
        void get_predicate_relations(std::shared_ptr<Ra__Node> predicate, std::vector<std::string>& relations);

        /**
         * Get relations referenced in expression
         * @param expression expression to check
         * @param relations vector to fill with referenced relations
         */
        void get_expression_relations(std::shared_ptr<Ra__Node> expression, std::vector<std::string>& relations);

        /**
         * Get relation referenced by attribute
         * @param attribute expression to check
         * @return relation name/alias
         */
        std::string get_relation_from_attribute(std::shared_ptr<Ra__Node> attribute);

        /**
         * gets relations and alias in top from clause layer of subtree
         * @param it pointer to subtree
         * @param relations_aliases vector to fill with relation name and alias defined in subtree (top layer)
         */
        void get_relations_aliases(std::shared_ptr<Ra__Node> it, std::vector<std::pair<std::string,std::string>>& relations_aliases);

        /**
         * finds first selection in subtree
         * @param it pointer to subtree
         * @return true if selection found, else false
         */
        bool get_first_selection(std::shared_ptr<Ra__Node>& it);
        
        /**
         * get all attributes used in subtree
         * @param it pointer to subtree
         * @param attributes vector to fill with found attribute nodes
         */
        void get_subtree_attributes(std::shared_ptr<Ra__Node> it, std::vector<std::shared_ptr<Ra__Node>>& attributes);

        /**
         * Checks if an attribute name belongs to a tpch relation
         * @param attr_name attribute name
         * @param relation relation name
         * @return true if attribute belongs to tpch relation
         */
        bool is_tpch_attribute(std::string attr_name, std::string relation);

        /**
         * Get tpch relation name of a tpch attribute
         * @param attr_name attribute name
         * @return corresponding tpch relation name
         */
        std::string get_tpch_relation_name(std::string attr_name);

        /**
         * Get attributes used in an expression
         * @param expression expression node
         * @param attributes vector to fill with attributes found
         */
        void get_expression_attributes(std::shared_ptr<Ra__Node> expression, std::vector<std::shared_ptr<Ra__Node>>& attributes);

        /**
         * Get attributes used in a predicate
         * @param predicate predicate node
         * @param attributes vector to fill with attributes found
         */
        void get_predicate_attributes(std::shared_ptr<Ra__Node> predicate, std::vector<std::shared_ptr<Ra__Node>>& attributes);

        /**
         * Check if a dependent join can be decoupled
         * @param dep_join pointer to dependent join node
         * @param parent_projection pointer to projection which has dep join in subtree
         * @param d_rename_map map "d" attributes to equivalent attributes in subtree
         * @return true if can be decoupled, else false
         */
        bool can_decouple(std::shared_ptr<Ra__Node__Join> dep_join, std::shared_ptr<Ra__Node> parent_projection, std::map<std::pair<std::string,std::string>, std::pair<std::string,std::string>>& d_rename_map);

        /**
         * Check if a dependent join can be decoupled
         * @param it pointer to subtree in which to rename attributes
         * @param rename_map map from old name to new name
         * @param stop_node node at which to stop renaming
         */
        void rename_attributes(std::shared_ptr<Ra__Node> it, std::map<std::pair<std::string,std::string>, std::pair<std::string,std::string>> rename_map, std::shared_ptr<Ra__Node> stop_node=nullptr);

        /**
         * Remove any redundant predicates within a predicate (redunant: a=a)
         * @param predicate predicate in which to remove redundant sub-predicates
         * @param predicate_parent pointer to parent of predicate, needed in case entire predicate becomes rendundant
         */
        void remove_redundant_predicates(std::shared_ptr<Ra__Node>& predicate, std::shared_ptr<Ra__Node>& predicate_parent);

        /**
         * Checks if an attribute has an equi predicate to another attribute
         * @param attribute attribute to check
         * @param predicate predicate in which to check for equi predicate
         * @param d_rename_map map in which original attribute is mapped to equivalent predicate
         * @return true if attribute has an equivalent attribute, else false
         */
        bool has_equivalent_attribute(std::shared_ptr<Ra__Node> attribute, std::shared_ptr<Ra__Node> predicate, std::map<std::pair<std::string,std::string>, std::pair<std::string,std::string>>& d_rename_map);

        /**
         * finds highest node in subtree where only the required relations are defined
         * @param it pointer to subtree
         * @param required_relations set of required relations
         * @return true if node is found
         */
        bool find_highest_node_with_only_required_relations_defined(std::shared_ptr<Ra__Node>& it, std::set<std::string> required_relations);

        /**
         * Creates a Ra Relation Node with tpch relation name corresponding to attribute name
         * @param alias alias to give to created Ra Relation Node
         * @param attr attribute for which to create corresponding tpch relation node
         * @return tpch Ra relation node
         */
        std::shared_ptr<Ra__Node> build_tpch_relation(std::string alias, std::vector<std::shared_ptr<Ra__Node__Attribute>> attr);

        /**
         * Find the parent node of a join node in subtree
         * @param it pointer to subtree, will equal parent of seeked join node if returned true
         * @return true if parent node was found
         */
        bool get_join_parent(std::shared_ptr<Ra__Node>& it);

        /**
         * Find the parent node of a join marker in subtree
         * @param it pointer to subtree, will equal parent of seeked join marker if returned true
         * @param marker join marker whose parent to find
         * @param index will be set to child index which join marker has in parent node
         * @return true if parent node was found
         */
        bool get_join_marker_parent(std::shared_ptr<Ra__Node>& it, std::shared_ptr<Ra__Node__Where_Subquery_Marker> marker, int& index);
        
        /**
         * Get all attributes in subtree which use given aliases
         * @param it pointer to subtree
         * @param aliases aliases to match attributes to
         * @param attributes vector to fill with all attributes found which use given aliases
         * @param stop_node node at which to stop looking for attributes
         * @param incl_stop_node true, if the stop node should be included in the search
         */
        void find_attributes_using_alias(std::shared_ptr<Ra__Node> it, std::vector<std::string>& aliases, std::vector<std::shared_ptr<Ra__Node>>& attributes, std::shared_ptr<Ra__Node> stop_node=nullptr, bool incl_stop_node=false);

        /**
         * checks if a predicate contains a subquery
         * @param node predicate in which to look for subquery
         * @return true if contains subquery, else false
         */
        bool predicate_contains_subquery(std::shared_ptr<Ra__Node> node);
};

#endif