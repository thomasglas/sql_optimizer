#include "ra_tree.h"
#include <tuple>
#include <algorithm>

RaTree::RaTree(std::shared_ptr<Ra__Node> _root, std::vector<std::shared_ptr<Ra__Node>> _ctes, uint64_t _counter)
:root(_root), ctes(_ctes), counter(_counter){
}

void RaTree::optimize(){
    push_down_predicates(false);
    // decorrelate_all_exists();
    decorrelate_arbitrary_queries();
    push_down_predicates(convert_cp_to_join);
}

void RaTree::push_down_predicates(bool cp_to_join){

    // after 1.3
    // check if split predicate is a correlating predicate
    // is yes: remove correlated relation from relations needed

    // 1. extract splittable predicates of all selections
        // 1.1 find all selections in tree, for each selection:
        // 1.2 split predicates 
        // 1.3 for each predicate: find all relations needed
        // 1.4 pop predicate
        // 1.5 save pointer to selection and all extracted predicates
    // 2. find new node for predicates
        // 2.1 in subtree of original selection: if find a lower node has all relations defined:
            // add predicate above node, remove from original selection
        // 2.2 if no lower node found with relations defined, re-add predicate to selection as boolean AND
        // 2.3 if all predicates have new node, remove old selection

    // 1.1
    std::vector<std::shared_ptr<Ra__Node>> selections;
    get_all_selections(root, selections);

    // vector(selections),Ra__Node(sel child),vec(predicates),Ra__Node(predicate),vector(relations),string(rel_id)
    std::vector<std::pair<std::shared_ptr<Ra__Node>,std::vector<std::pair<std::shared_ptr<Ra__Node>,std::vector<std::string>>>>> selections_predicates_relations;
    for(const auto& selection: selections){
        auto sel = std::static_pointer_cast<Ra__Node__Selection>(selection);
        std::vector<std::pair<std::shared_ptr<Ra__Node>,std::vector<std::string>>> predicates_relations; // splitted predicates, and relations referenced

        // 1.2
        split_selection_predicates(sel->predicate, predicates_relations);

        // 1.3
        for(auto& p_r: predicates_relations){
            get_predicate_relations(p_r.first, p_r.second);
        }

        if(push_down_correlating_predicates){
            // preparing pushing down correlated predicates -> remove correlated relation for required relations_aliases
            // get relations defined for selection
            std::vector<std::pair<std::string,std::string>> defined_relations_aliases;
            get_relations_aliases(selection->childNodes[0], defined_relations_aliases);
            for(auto& p_r: predicates_relations){
                // check if predicate is correlating
                bool dummy_is_boolean_predicate;
                std::vector<std::tuple<std::shared_ptr<Ra__Node>,std::shared_ptr<Ra__Node>,std::string,size_t>> correlating_predicates;
                // is_correlating_predicate(std::static_pointer_cast<Ra__Node__Predicate>(p_r.first), defined_relations_aliases);
                get_correlating_predicates(p_r.first, correlating_predicates, dummy_is_boolean_predicate, defined_relations_aliases);
                
                // remove correlated predicate relations from required relations_aliases
                for(const auto& correlating_predicate: correlating_predicates){
                    std::string relation_alias = get_relation_from_attribute(std::get<0>(correlating_predicate));
                    p_r.second.erase(
                        std::remove_if(p_r.second.begin(),p_r.second.end(),[&](std::string const& required_name){return required_name==relation_alias;}), 
                        p_r.second.end()
                    );
                }
            }
        }

        // 1.4 (will either find new node, or readded to this selection)
        sel->predicate = nullptr;

        // 1.5
        selections_predicates_relations.push_back({sel, predicates_relations});
    }

    // all selections are removed from tree
    // now insert extracted predicates at lowest node possible, add selection nodes as needed
    for(const auto& selection: selections_predicates_relations){
        for(const auto& predicate_relations: selection.second){
            // 2.1
            if(!find_lowest_child_insert_selection(predicate_relations, selection.first, cp_to_join)){
                // 2.2
                add_predicate_to_selection(predicate_relations.first, selection.first);
            }
        }
        // 2.3
        if(std::static_pointer_cast<Ra__Node__Selection>(selection.first)->predicate==nullptr){
            std::shared_ptr<Ra__Node> sel_parent = root;
            int child_index = -1;
            get_node_parent(sel_parent, selection.first, child_index);
            sel_parent->childNodes[child_index] = selection.first->childNodes[0];
            selection.first->childNodes.pop_back();
        }
    }
}

void RaTree::add_predicate_to_selection(std::shared_ptr<Ra__Node> predicate, std::shared_ptr<Ra__Node> selection){
    auto sel = std::static_pointer_cast<Ra__Node__Selection>(selection);
    if(sel->predicate==nullptr){
        sel->predicate = predicate;
    }
    else if(sel->predicate->node_case==RA__NODE__BOOL_PREDICATE){
        auto bool_p = std::static_pointer_cast<Ra__Node__Bool_Predicate>(sel->predicate);
        if(bool_p->bool_operator==RA__BOOL_OPERATOR__AND){
            bool_p->args.push_back(predicate);
        }
        else{
            auto bool_p = std::make_shared<Ra__Node__Bool_Predicate>();
            bool_p->bool_operator = RA__BOOL_OPERATOR__AND;
            bool_p->args.push_back(sel->predicate);
            bool_p->args.push_back(predicate);
            sel->predicate = bool_p;
        }
    }
    else if(sel->predicate->node_case==RA__NODE__PREDICATE
        || sel->predicate->node_case==RA__NODE__NULL_TEST){
        auto bool_p = std::make_shared<Ra__Node__Bool_Predicate>();
        bool_p->bool_operator = RA__BOOL_OPERATOR__AND;
        bool_p->args.push_back(sel->predicate);
        bool_p->args.push_back(predicate);
        sel->predicate = bool_p;
    }
}

void RaTree::add_predicate_to_join(std::shared_ptr<Ra__Node> predicate, std::shared_ptr<Ra__Node> join_node){
    auto join = std::static_pointer_cast<Ra__Node__Join>(join_node);
    if(join->type==RA__JOIN__CROSS_PRODUCT){
        join->type = RA__JOIN__INNER;
    }
    if(join->predicate==nullptr){
        join->predicate = predicate;
    }
    else if(join->predicate->node_case==RA__NODE__BOOL_PREDICATE){
        auto bool_p = std::static_pointer_cast<Ra__Node__Bool_Predicate>(join->predicate);
        if(bool_p->bool_operator==RA__BOOL_OPERATOR__AND){
            bool_p->args.push_back(predicate);
        }
        else{
            auto bool_p = std::make_shared<Ra__Node__Bool_Predicate>();
            bool_p->bool_operator = RA__BOOL_OPERATOR__AND;
            bool_p->args.push_back(join->predicate);
            bool_p->args.push_back(predicate);
            join->predicate = bool_p;
        }
    }
    else if(join->predicate->node_case==RA__NODE__PREDICATE
        || join->predicate->node_case==RA__NODE__NULL_TEST){
        auto bool_p = std::make_shared<Ra__Node__Bool_Predicate>();
        bool_p->bool_operator = RA__BOOL_OPERATOR__AND;
        bool_p->args.push_back(join->predicate);
        bool_p->args.push_back(predicate);
        join->predicate = bool_p;
    }
}

bool RaTree::predicate_contains_subquery(std::shared_ptr<Ra__Node> node){
    switch(node->node_case){
        case RA__NODE__BOOL_PREDICATE:{
            auto bool_p = std::static_pointer_cast<Ra__Node__Bool_Predicate>(node);
            bool found_subquery = false;
            for(auto arg: bool_p->args){
                if(!found_subquery){
                    found_subquery = predicate_contains_subquery(arg);
                }
            }
            return found_subquery;
        }
        case RA__NODE__PREDICATE:{
            auto p = std::static_pointer_cast<Ra__Node__Predicate>(node);
            return predicate_contains_subquery(p->left) || predicate_contains_subquery(p->right);
        }
        case RA__NODE__WHERE_SUBQUERY_MARKER:{
            return true;
        }
        default: return false;
    }
}

bool RaTree::find_lowest_child_insert_selection(const std::pair<std::shared_ptr<Ra__Node>,std::vector<std::string>>& predicate_relations, std::shared_ptr<Ra__Node> selection, bool cp_to_join){
    // keep pointer to parent, iterate through tree
    // from bottom, check relations and joins for which relations are defined
    // if all relations needed for predicate are defined, add selection above join/relation
        // if parent is already selection

    // 1. find if lower node has all relations defined
    std::shared_ptr<Ra__Node> it = selection->childNodes[0];
    std::shared_ptr<Ra__Node> child_it = nullptr;
    if(find_node_where_relations_defined(it, predicate_relations.second)){
        if(it->node_case==RA__NODE__JOIN && cp_to_join && !predicate_contains_subquery(predicate_relations.first)){
            add_predicate_to_join(predicate_relations.first, it);
        }
        else{
            int child_index = -1;
            std::shared_ptr<Ra__Node> parent = root;
            get_node_parent(parent, it, child_index);
            // 2. insert selection
            if(parent->node_case==RA__NODE__SELECTION){
                // if parent is already selection, add predicate to it
                add_predicate_to_selection(predicate_relations.first, parent);
            }
            else{
                // insert new selection node
                auto sel = std::make_shared<Ra__Node__Selection>();
                sel->predicate = predicate_relations.first;
                sel->childNodes.push_back(parent->childNodes[child_index]);
                parent->childNodes[child_index] = sel;
            }
        }
        return true;
    }
    return false;
}

bool RaTree::find_node_where_relations_defined(std::shared_ptr<Ra__Node>& it, const std::vector<std::string>& relations){
    
    // if relations defined here, check if also defined in children
    if(has_relations_defined(it, relations)){
        bool found = false;
        std::shared_ptr<Ra__Node> temp_this_node = it;
        auto childNodes = it->childNodes;
        for(int i=0; i<childNodes.size(); i++){
            if(!found){
                it = childNodes[i];
                if(find_node_where_relations_defined(it, relations)){
                    found = true;
                }
            }
        }
        // current node defines relation, none of children do
        if(!found){
            it = temp_this_node;
        }
        return true;
    }
    return false;
}

bool RaTree::has_relations_defined(std::shared_ptr<Ra__Node> node, const std::vector<std::string>& relations){
    std::vector<std::pair<std::string,std::string>> defined_relations_aliases;
    get_relations_aliases(node, defined_relations_aliases);

    for(const auto& needed_relation: relations){
        bool is_defined = false;
        for(const auto& defined_relation: defined_relations_aliases){
            if(needed_relation==defined_relation.first || needed_relation==defined_relation.second){
                is_defined = true;
                break;
            }
        }
        if(!is_defined){
            return false;
        }
    }
    return true;
}

// fill relations vector with all relations referenced by predicate
void RaTree::get_predicate_relations(std::shared_ptr<Ra__Node> predicate, std::vector<std::string>& relations){
    switch(predicate->node_case){
        case RA__NODE__PREDICATE:{
            auto p = std::static_pointer_cast<Ra__Node__Predicate>(predicate);
            get_expression_relations(p->left, relations);
            get_expression_relations(p->right, relations);
            break;
        }
        case RA__NODE__WHERE_SUBQUERY_MARKER:{
            auto marker = std::static_pointer_cast<Ra__Node__Where_Subquery_Marker>(predicate);
            get_expression_relations(marker, relations);
            break;
        }
        case RA__NODE__BOOL_PREDICATE:{
            auto bool_p = std::static_pointer_cast<Ra__Node__Bool_Predicate>(predicate);
            for(const auto& arg: bool_p->args){
                get_predicate_relations(arg, relations);
            }
            break;
        }
        case RA__NODE__NULL_TEST:{
            auto null_test = std::static_pointer_cast<Ra__Node__Null_Test>(predicate);
            get_expression_relations(null_test->arg, relations);
            break;
        }
        default: std::cout << "node case should not be in split predicates" << std::endl;
    }
    
}

void RaTree::get_expression_relations(std::shared_ptr<Ra__Node> expression, std::vector<std::string>& relations){
    switch(expression->node_case){
        case RA__NODE__ATTRIBUTE:{
            relations.push_back(get_relation_from_attribute(expression));
            break;
        }
        case RA__NODE__EXPRESSION:{
            auto expr = std::static_pointer_cast<Ra__Node__Expression>(expression);
            get_expression_relations(expr->l_arg, relations);
            get_expression_relations(expr->r_arg, relations);
            break;
        }
        case RA__NODE__FUNC_CALL:{
            auto func_call = std::static_pointer_cast<Ra__Node__Func_Call>(expression);
            for(const auto& arg: func_call->args){
                get_expression_relations(arg, relations);
            }
            break;
        }
        case RA__NODE__TYPE_CAST:{
            auto type_cast = std::static_pointer_cast<Ra__Node__Type_Cast>(expression);
            get_expression_relations(type_cast->expression, relations);
            break;
        }
        case RA__NODE__WHERE_SUBQUERY_MARKER:{
            auto marker = std::static_pointer_cast<Ra__Node__Where_Subquery_Marker>(expression);
            relations.push_back("marker_"+std::to_string(marker->marker));
            break;
        }
        default: break; // const
    }
}

std::string RaTree::get_relation_from_attribute(std::shared_ptr<Ra__Node> attribute){
    auto attr = std::static_pointer_cast<Ra__Node__Attribute>(attribute);
    // if has alias, return alias
    if(attr->alias.length()>0){
        return attr->alias;
    }
    else{
        // if no alias, refer through tpch name
        return get_tpch_relation_name(attr->name);
    }
}

std::string RaTree::get_tpch_relation_name(std::string attr_name){
    std::string::size_type pos = attr_name.find('_');
    std::string attr_prefix = attr_name.substr(0, pos);

    if(attr_prefix=="p"){
        return "part";
    }
    else if(attr_prefix=="s"){
        return "supplier";
    }
    else if(attr_prefix=="ps"){
        return "partsupp";
    }
    else if(attr_prefix=="c"){
        return "customer";
    }
    else if(attr_prefix=="n"){
        return "nation";
    }
    else if(attr_prefix=="l"){
        return "lineitem";
    }
    else if(attr_prefix=="r"){
        return "region";
    }
    else if(attr_prefix=="o"){
        return "orders";
    }
    else{
        std::cout << "attribute is not tpch" << std::endl;
        return "";
    }
}

// split by "and"
void RaTree::split_selection_predicates(std::shared_ptr<Ra__Node> predicate, std::vector<std::pair<std::shared_ptr<Ra__Node>,std::vector<std::string>>>& predicates_relations){
    switch(predicate->node_case){
        case RA__NODE__WHERE_SUBQUERY_MARKER:
        case RA__NODE__NULL_TEST:
        case RA__NODE__PREDICATE:{
            predicates_relations.push_back({predicate,{}});
            break;
        }
        case RA__NODE__BOOL_PREDICATE:{
            auto bool_p = std::static_pointer_cast<Ra__Node__Bool_Predicate>(predicate);
            switch(bool_p->bool_operator){
                case RA__BOOL_OPERATOR__AND:{
                    for(const auto& arg: bool_p->args){
                        split_selection_predicates(arg, predicates_relations);
                    }
                    break;
                }
                case RA__BOOL_OPERATOR__OR:
                case RA__BOOL_OPERATOR__NOT:{
                    // can't split predicate
                    predicates_relations.push_back({predicate,{}});
                    break;
                }
            }
            break;
        }
        default: std::cout << "this node case should not be in selection predicate" << std::endl;
    }
}

void RaTree::decorrelate_arbitrary_queries(){
    // find marker in selection
    std::vector<std::pair<std::shared_ptr<Ra__Node>, std::shared_ptr<Ra__Node>>> markers_joins; // markers, joins
    
    // 1.
    find_subquery_markers(root, markers_joins, RA__JOIN__DEPENDENT_INNER_LEFT);

    // 2.
    find_joins_by_markers(root, markers_joins);

    // 3. loop in reverse, to decorrelate most nested exists first
    for(int i=markers_joins.size()-1; i>=0; i--){
        // assert each marker found corresponding join
        assert(markers_joins[i].second!=nullptr);
        decorrelate_subquery(markers_joins[i]);
    };
}

void RaTree::decorrelate_subquery(std::pair<std::shared_ptr<Ra__Node>, std::shared_ptr<Ra__Node>> markers_joins){

    // 0. Replace selection marker with produced attribute
    // 1. replace dep join with inner join, remove marker
        // 1.1 compute join predicate
        // 1.2 add alias and columns to right side projection
    // 2. add dep join to join right child
        // 2.1 compute left child D:
            // 2.1.1 subquery from top join left side child
            // 2.1.2 compute which attritbues to select -> add to d projection and right projection
        // 2.2 in right child, replace all outer query aliases with d
    // 3. push down dep join
        // 3.1 convert dep join to cp
    // 4. Decouple (if possible: only correlated equi predicates (no expressions supported))
        // 4.1 remove join 
        // 4.2 rename all d attribute alias to their original alias (whole right side)
        // 4.3 remove a=a predicates
    // 5. If can't decouple, move to CTE
        // TODO: rename attributes to unique names in projection, create rename map for whole query
        // 5.1 create projection on left side, add to CTEs
        // 5.2 get relations/aliases used in CTE
        // 5.3 go through tree, find attributes used from CTE, add to CTE select expressions
        // 5.4 point d to CTE, point original dep join left to CTE
        // 5.5 go through whole tree, rename all attributes with CTE relations/alises

    // 0. replace selection marker with produced attribute
    std::shared_ptr<Ra__Node> it = root;
    auto marker = std::static_pointer_cast<Ra__Node__Where_Subquery_Marker>(markers_joins.first);
    int child_index = -1;
    std::string subquery_alias = "t" + std::to_string(counter++);
    assert(find_marker_parent(it, marker, child_index));
    replace_subquery_marker(it, child_index, std::make_shared<Ra__Node__Attribute>("m",subquery_alias));
    
    // 1.
    auto original_dep_join = std::static_pointer_cast<Ra__Node__Join>(markers_joins.second);
    original_dep_join->type = RA__JOIN__CROSS_PRODUCT;
    original_dep_join->right_where_subquery_marker->marker = 0;

    // 1.1
    assert(original_dep_join->predicate==nullptr);

    // 1.2
    auto right_projection = std::static_pointer_cast<Ra__Node__Projection>(original_dep_join->childNodes[1]);
    right_projection->subquery_alias = subquery_alias;
    right_projection->subquery_columns.push_back("m"); 

    // 2.
    auto dep_join = std::make_shared<Ra__Node__Join>(RA__JOIN__DEPENDENT_INNER_LEFT);
    dep_join->childNodes.resize(2);
    dep_join->childNodes[1] = right_projection->childNodes[0];
    right_projection->childNodes[0] = dep_join;

    // add selection above original dep join for the join predicate (using cross product)
    child_index = -1;
    std::shared_ptr<Ra__Node> original_dep_join_selection = root;
    assert(get_node_parent(original_dep_join_selection, original_dep_join, child_index));
    if(original_dep_join_selection->node_case!=RA__NODE__SELECTION){
        auto sel = std::make_shared<Ra__Node__Selection>();
        sel->childNodes.push_back(original_dep_join_selection->childNodes[child_index]);
        original_dep_join_selection->childNodes[child_index] = sel;
    }

    // 2.1
    auto d_projection = std::make_shared<Ra__Node__Projection>();
    d_projection->childNodes.push_back(original_dep_join->childNodes[0]);
    d_projection->subquery_alias = "d";
    dep_join->childNodes[0] = d_projection;
    auto correlated_attributes = intersect_correlated_attributes(dep_join->childNodes[0],dep_join->childNodes[1]->childNodes[0]);
    std::vector<std::string> temp_duplicate_tracker;
    for(auto& correlated_attribute: correlated_attributes){
        auto attr = std::static_pointer_cast<Ra__Node__Attribute>(correlated_attribute);

        // duplicate check
        if(std::find(temp_duplicate_tracker.begin(),temp_duplicate_tracker.end(),attr->alias+attr->name)==temp_duplicate_tracker.end()){
            temp_duplicate_tracker.push_back(attr->alias+attr->name);

            right_projection->args.push_back(std::make_shared<Ra__Node__Attribute>(attr->name, "d"));
        
            // 2.1.2
            d_projection->args.push_back(std::make_shared<Ra__Node__Attribute>(attr->name));
            d_projection->subquery_columns.push_back(attr->name);

            // 1.1 
            add_predicate_to_selection(std::make_shared<Ra__Node__Predicate>(std::make_shared<Ra__Node__Attribute>(attr->name, attr->alias),std::make_shared<Ra__Node__Attribute>(subquery_alias+"_"+attr->name, subquery_alias),"="), original_dep_join_selection);
            right_projection->subquery_columns.push_back(subquery_alias+"_"+attr->name);
        }
        
        // 2.2
        attr->alias = "d";
    }

    // pushing down
        // queue of dep joins
        // while not empty -> push down dep join
        // when done pushing down a dep join, add to vector, pop from queue
    std::vector<std::shared_ptr<Ra__Node__Join>> dep_joins_vector;
    std::queue<std::shared_ptr<Ra__Node__Join>> push_down_dep_joins_queue; 
    push_down_dep_joins_queue.push(dep_join);
    while(!push_down_dep_joins_queue.empty()){
        std::shared_ptr<Ra__Node__Join> dep_join_ = push_down_dep_joins_queue.front();
        while(!intersect_correlated_attributes(dep_join_->childNodes[0],dep_join_->childNodes[1]).empty()){
            // DONE: change push_down to take dep join and find parent inside the function
            // DONE: add push_down takes dep_join_queue
            // DONE: change case 3 behavior, add new dep_join to dep_join_queue
            push_down_dep_join(dep_join_, right_projection, push_down_dep_joins_queue);
        }
        dep_joins_vector.push_back(dep_join_);
        push_down_dep_joins_queue.pop();
    }


    // decouple
        // loop throuh dep join vector
        // decouple if possible: check "d" predicates going up to first parent projection
        // set flag if can't decouple
    bool cte_already_setup = false;
    std::map<std::pair<std::string,std::string>, std::pair<std::string,std::string>> rename_d_attributes;
    for(auto dep_join_: dep_joins_vector){
        dep_join_->type = RA__JOIN__CROSS_PRODUCT;

        // find dep join parent
        int dep_join_parent_child_id = 0;
        std::shared_ptr<Ra__Node> dep_join_parent = right_projection;
        assert(get_node_parent(dep_join_parent, dep_join_, dep_join_parent_child_id));

        // find parent projection of dep join 
        std::shared_ptr<Ra__Node> parent_projection = right_projection;
        std::shared_ptr<Ra__Node> find_parent_of = dep_join_parent->childNodes[dep_join_parent_child_id];
        int child_index;
        while(get_node_parent(parent_projection, find_parent_of, child_index)){
            if(parent_projection->node_case==RA__NODE__PROJECTION){
                break;
            }
            find_parent_of = parent_projection;
            parent_projection = right_projection;
        }

        if(decouple && can_decouple(dep_join_, parent_projection, rename_d_attributes)){

            // remove join
            dep_join_parent->childNodes[dep_join_parent_child_id] = dep_join_parent->childNodes[dep_join_parent_child_id]->childNodes[1];

            // 4.2
            rename_attributes(parent_projection, rename_d_attributes);

            // 4.3
            std::vector<std::shared_ptr<Ra__Node>> selections;
            get_all_selections(parent_projection, selections);
            for(auto& sel: selections){
                remove_redundant_predicates(sel, sel);
            } 
        }
        // run through can't-decouple sequence: create CTE, point d_projection to CTE
        else{
            if(cte_already_setup){

            }
            else{
                cte_already_setup = true;
                // 5.1
                auto cte_projection = std::make_shared<Ra__Node__Projection>();
                cte_projection->childNodes.push_back(original_dep_join->childNodes[0]);
                std::string cte_name = "cte_" + std::to_string(counter++);
                cte_projection->subquery_alias = cte_name;
                ctes.push_back(cte_projection);

                // 5.2
                std::vector<std::pair<std::string, std::string>> cte_relations_aliases;
                get_relations_aliases(cte_projection->childNodes[0], cte_relations_aliases);

                // 5.4
                original_dep_join->childNodes[0] = std::make_shared<Ra__Node__Relation>(cte_name);
                d_projection->childNodes[0] = original_dep_join->childNodes[0];

                // 5.3
                std::vector<std::shared_ptr<Ra__Node>> attributes;
                // extract either alias or relation
                std::vector<std::string> cte_aliases;
                for(const auto& cte_relation_alias: cte_relations_aliases){
                    // relation has alias
                    if(cte_relation_alias.second!=""){
                        cte_aliases.push_back(cte_relation_alias.second);
                    }
                    // relation has no alias
                    else{
                        cte_aliases.push_back(cte_relation_alias.first);
                    }
                }
                find_attributes_using_alias(this->root, cte_aliases, attributes);
                std::vector<std::string> temp_select_attr_tracker;
                for(auto attribute: attributes){
                    auto attr = std::static_pointer_cast<Ra__Node__Attribute>(attribute);
                    if(std::find(temp_select_attr_tracker.begin(),temp_select_attr_tracker.end(),attr->alias+attr->name)==temp_select_attr_tracker.end()){
                        temp_select_attr_tracker.push_back(attr->alias+attr->name);
                        cte_projection->args.push_back(std::make_shared<Ra__Node__Select_Expression>(std::make_shared<Ra__Node__Attribute>(attr->name, attr->alias)));
                    }
                    // attr->alias = cte_name;
                }

                // 5.5
                std::map<std::pair<std::string,std::string>, std::pair<std::string,std::string>> rename_map;
                for(const auto& cte_relation_alias: cte_relations_aliases){
                    // relation has alias
                    if(cte_relation_alias.second!=""){
                        rename_map[{cte_relation_alias.second,""}] = {cte_name,""};
                    }
                    // relation has no alias
                    else{
                        rename_map[{cte_relation_alias.first,""}] = {cte_name,""};
                    }
                }
                rename_attributes(root, rename_map);
            }
        }
    }
}

void RaTree::find_attributes_using_alias(std::shared_ptr<Ra__Node> it, std::vector<std::string>& aliases, std::vector<std::shared_ptr<Ra__Node>>& attributes, std::shared_ptr<Ra__Node> stop_node, bool incl_stop_node){
    if(it==stop_node && !incl_stop_node){
        return;
    }
    switch(it->node_case){
        case RA__NODE__ATTRIBUTE:{
            auto attr = std::static_pointer_cast<Ra__Node__Attribute>(it);
            if(std::find(aliases.begin(),aliases.end(),attr->alias)!=aliases.end()){
                attributes.push_back(it);
            }
            else if(attr->alias==""){ 
                for(const auto& alias: aliases){
                    if(is_tpch_attribute(attr->name, alias)){
                        attributes.push_back(it);
                        break;
                    }
                }
            }
            break;
        }
        case RA__NODE__FUNC_CALL:{
            auto func_call = std::static_pointer_cast<Ra__Node__Func_Call>(it);
            for(auto& arg: func_call->args){
                find_attributes_using_alias(arg, aliases, attributes, stop_node, incl_stop_node);
            }
            break;
        }
        case RA__NODE__PROJECTION:{
            auto p = std::static_pointer_cast<Ra__Node__Projection>(it);
            for(auto& arg: p->args){
                find_attributes_using_alias(arg, aliases, attributes, stop_node, incl_stop_node);
            }
            break;
        }
        case RA__NODE__SELECTION:{
            auto sel = std::static_pointer_cast<Ra__Node__Selection>(it);
            find_attributes_using_alias(sel->predicate, aliases, attributes, stop_node, incl_stop_node);
            break;
        }
        case RA__NODE__BOOL_PREDICATE:{
            auto bool_p = std::static_pointer_cast<Ra__Node__Bool_Predicate>(it);
            for(auto& arg: bool_p->args){
                find_attributes_using_alias(arg, aliases, attributes, stop_node, incl_stop_node);
            }
            break;
        }
        case RA__NODE__PREDICATE:{
            auto p = std::static_pointer_cast<Ra__Node__Predicate>(it);
            find_attributes_using_alias(p->left, aliases, attributes, stop_node, incl_stop_node);
            find_attributes_using_alias(p->right, aliases, attributes, stop_node, incl_stop_node);
            break;
        }
        case RA__NODE__GROUP_BY:{
            auto group_by = std::static_pointer_cast<Ra__Node__Group_By>(it);
            for(auto& arg: group_by->args){
                find_attributes_using_alias(arg, aliases, attributes, stop_node, incl_stop_node);
            }
            break;
        }
        case RA__NODE__JOIN:{
            auto join = std::static_pointer_cast<Ra__Node__Join>(it);
            if(join->predicate!=nullptr){
                find_attributes_using_alias(join->predicate, aliases, attributes, stop_node, incl_stop_node);
            }
            break;
        }
        case RA__NODE__SELECT_EXPRESSION:{
            auto sel_expr = std::static_pointer_cast<Ra__Node__Select_Expression>(it);
            find_attributes_using_alias(sel_expr->expression, aliases, attributes, stop_node, incl_stop_node);
            break;
        }
        case RA__NODE__ORDER_BY:{
            auto order_by = std::static_pointer_cast<Ra__Node__Order_By>(it);
            for(auto& arg: order_by->args){
                find_attributes_using_alias(arg, aliases, attributes, stop_node, incl_stop_node);
            }
            break;
        }
        case RA__NODE__EXPRESSION:{
            auto expr = std::static_pointer_cast<Ra__Node__Expression>(it);
            find_attributes_using_alias(expr->l_arg, aliases, attributes, stop_node, incl_stop_node);
            find_attributes_using_alias(expr->r_arg, aliases, attributes, stop_node, incl_stop_node);
            break;
        }
        case RA__NODE__TYPE_CAST:{
            auto type_cast = std::static_pointer_cast<Ra__Node__Type_Cast>(it);
            find_attributes_using_alias(type_cast->expression, aliases, attributes, stop_node, incl_stop_node);
            break;
        }
        default: {
            return;
        }
    }
    if(it==stop_node){
        return;
    }
    for(auto child: it->childNodes){
        find_attributes_using_alias(child, aliases, attributes, stop_node, incl_stop_node);
    }
}

void RaTree::remove_redundant_predicates(std::shared_ptr<Ra__Node>& predicate, std::shared_ptr<Ra__Node>& predicate_parent){
    switch(predicate->node_case){
        case RA__NODE__SELECTION:{
            auto sel = std::static_pointer_cast<Ra__Node__Selection>(predicate);
            remove_redundant_predicates(sel->predicate, predicate);
            if(sel->predicate==nullptr){
                std::shared_ptr<Ra__Node> sel_parent = root;
                int child_index = -1;
                assert(get_node_parent(sel_parent, sel, child_index));
                sel_parent->childNodes[child_index] = sel->childNodes[0];
                sel->childNodes.clear();
            }
            break;
        }
        case RA__NODE__BOOL_PREDICATE:{
            auto bool_p = std::static_pointer_cast<Ra__Node__Bool_Predicate>(predicate);
            for(auto& arg: bool_p->args){
                // sets predicates to nullptr
                remove_redundant_predicates(arg, predicate);
            }
            // remove all nullptr predicates
            bool_p->args.erase(
                std::remove_if(bool_p->args.begin(),bool_p->args.end(),[](std::shared_ptr<Ra__Node> const& p){return p==nullptr;}), 
                bool_p->args.end()
            );
            // if only 1 predicate left, turn into normal predicate
            if(bool_p->args.size()==1){
                predicate_parent = bool_p->args[0];
            }
            // if 0 predicates left, remove boolean predicate
            else if(bool_p->args.size()==0){
                predicate = nullptr;
            }
            break;
        }
        case RA__NODE__PREDICATE:{
            auto p = std::static_pointer_cast<Ra__Node__Predicate>(predicate);
            if(p->left->node_case==RA__NODE__ATTRIBUTE && p->right->node_case==RA__NODE__ATTRIBUTE){
                auto attr_l = std::static_pointer_cast<Ra__Node__Attribute>(p->left);
                auto attr_r = std::static_pointer_cast<Ra__Node__Attribute>(p->right);
                if(attr_l->name==attr_r->name && attr_l->alias==attr_r->alias){
                    predicate = nullptr;
                }
            }
            break;
        }
        default: return;
    }
}

// rename attributes in projection and selection
void RaTree::rename_attributes(std::shared_ptr<Ra__Node> it, std::map<std::pair<std::string,std::string>, std::pair<std::string,std::string>> rename_map, std::shared_ptr<Ra__Node> stop_node){
    if(it==stop_node){
        return;
    }
    
    switch(it->node_case){
        case RA__NODE__ATTRIBUTE:{
            auto attr = std::static_pointer_cast<Ra__Node__Attribute>(it);
            auto rename = rename_map.find({attr->alias,attr->name});
            if(rename!=rename_map.end()){
                attr->alias = rename->second.first;
                attr->name = rename->second.second;
            }
            else{
                // rename alias of attribute
                auto rename_all = rename_map.find({attr->alias,""});
                if(rename_all!=rename_map.end()){
                    attr->alias = rename_all->second.first;
                }
            }
            break;
        }
        case RA__NODE__FUNC_CALL:{
            auto func_call = std::static_pointer_cast<Ra__Node__Func_Call>(it);
            for(auto& arg: func_call->args){
                rename_attributes(arg, rename_map, stop_node);
            }
            break;
        }
        case RA__NODE__PROJECTION:{
            auto p = std::static_pointer_cast<Ra__Node__Projection>(it);
            for(auto& arg: p->args){
                rename_attributes(arg, rename_map, stop_node);
            }
            rename_attributes(p->childNodes[0], rename_map, stop_node);
            break;
        }
        case RA__NODE__SELECTION:{
            auto sel = std::static_pointer_cast<Ra__Node__Selection>(it);
            rename_attributes(sel->predicate, rename_map, stop_node);
            rename_attributes(sel->childNodes[0], rename_map, stop_node);
            break;
        }
        case RA__NODE__BOOL_PREDICATE:{
            auto bool_p = std::static_pointer_cast<Ra__Node__Bool_Predicate>(it);
            for(auto& arg: bool_p->args){
                rename_attributes(arg, rename_map, stop_node);
            }
            break;
        }
        case RA__NODE__PREDICATE:{
            auto p = std::static_pointer_cast<Ra__Node__Predicate>(it);
            rename_attributes(p->left, rename_map, stop_node);
            rename_attributes(p->right, rename_map, stop_node);
            break;
        }
        case RA__NODE__GROUP_BY:{
            auto group_by = std::static_pointer_cast<Ra__Node__Group_By>(it);
            for(auto& arg: group_by->args){
                rename_attributes(arg, rename_map, stop_node);
            }
            rename_attributes(group_by->childNodes[0], rename_map, stop_node);
            break;
        }
        case RA__NODE__JOIN:{
            auto join = std::static_pointer_cast<Ra__Node__Join>(it);
            if(join->predicate!=nullptr){
                rename_attributes(join->predicate, rename_map, stop_node);
            }
            rename_attributes(join->childNodes[0], rename_map, stop_node);
            rename_attributes(join->childNodes[1], rename_map, stop_node);
            break;
        }
        case RA__NODE__SELECT_EXPRESSION:{
            auto sel_expr = std::static_pointer_cast<Ra__Node__Select_Expression>(it);
            rename_attributes(sel_expr->expression, rename_map, stop_node);
            break;
        }
        default: return;
    }
}

// only has equivalent class if has an equi predicate in a top level "and" bool predicate
bool RaTree::has_equivalent_attribute(std::shared_ptr<Ra__Node> attribute, std::shared_ptr<Ra__Node> predicate, std::map<std::pair<std::string,std::string>, std::pair<std::string,std::string>>& d_rename_map){
    switch(predicate->node_case){
        case RA__NODE__BOOL_PREDICATE:{
            auto bool_p = std::static_pointer_cast<Ra__Node__Bool_Predicate>(predicate);
            if(bool_p->bool_operator==RA__BOOL_OPERATOR__AND){
                bool found_equivalence_class = false;
                for(const auto& arg: bool_p->args){
                    if(!found_equivalence_class){
                        found_equivalence_class = has_equivalent_attribute(attribute, arg, d_rename_map);
                    }
                }
                return found_equivalence_class;
            }
            else{
                return false;
            }
        }
        case RA__NODE__PREDICATE:{
            auto p = std::static_pointer_cast<Ra__Node__Predicate>(predicate);
            auto attr_d = std::static_pointer_cast<Ra__Node__Attribute>(attribute);
            if(p->binaryOperator=="=" && p->left->node_case==RA__NODE__ATTRIBUTE && p->right->node_case==RA__NODE__ATTRIBUTE){
                auto attr_l = std::static_pointer_cast<Ra__Node__Attribute>(p->left);
                auto attr_r = std::static_pointer_cast<Ra__Node__Attribute>(p->right);
                if((attr_l->alias=="d" && attr_l->name==attr_d->name && attr_r->alias!="d")){
                    d_rename_map[{"d",attr_d->name}] = {attr_r->alias,attr_r->name};
                    return true;
                }
                else if((attr_r->alias=="d" && attr_r->name==attr_d->name && attr_l->alias!="d")){
                    d_rename_map[{"d",attr_d->name}] = {attr_l->alias,attr_l->name};
                    return true;
                }
            }
            return false;
        }
        default: return false;
    }
}

// // can only decouple if every "d" attribute has an equi predicate in a top level "and" bool predicate
// // (can also have any <,> predicates additionally)
// bool RaTree::can_decouple(std::shared_ptr<Ra__Node> selection, const std::vector<std::shared_ptr<Ra__Node>>& d_args, std::map<std::pair<std::string,std::string>, std::pair<std::string,std::string>>& d_rename_map){
//     auto sel = std::static_pointer_cast<Ra__Node__Selection>(selection);
//     // every attribute passed from "d" must have an equi predicate
//     for(const auto& d_arg: d_args){
//         if(!has_equivalent_attribute(d_arg, sel->predicate, d_rename_map)){
//             return false;
//         };
//     }
//     return true;
// }

bool RaTree::can_decouple(std::shared_ptr<Ra__Node__Join> dep_join, std::shared_ptr<Ra__Node> parent_projection, std::map<std::pair<std::string,std::string>, std::pair<std::string,std::string>>& d_rename_map){
    
    // get d_args from left side, each d_arg needs a "=" predicate
    // get predicates by cheking dep_join, and all parents up to the first projection
    // for each d_arg, check if matching "=" exists
    auto d_projection = std::static_pointer_cast<Ra__Node__Projection>(dep_join->childNodes[0]);

    std::vector<std::shared_ptr<Ra__Node>> nodes_with_predicates;
    nodes_with_predicates.push_back(dep_join);

    std::shared_ptr<Ra__Node> parent_node = parent_projection;
    std::shared_ptr<Ra__Node> find_parent_of = dep_join;
    int child_index;
    while(get_node_parent(parent_node, find_parent_of, child_index)){
        if(parent_node->node_case==RA__NODE__SELECTION || parent_node->node_case==RA__NODE__JOIN){
            nodes_with_predicates.push_back(parent_node);
        }
        else if(parent_node->node_case==RA__NODE__PROJECTION){
            break;
        }
        find_parent_of = parent_node;
        parent_node = parent_projection;
    }

    std::vector<std::string> aliases_to_find = {"d"};
    std::vector<std::shared_ptr<Ra__Node>> attributes_with_alias;
    find_attributes_using_alias(parent_projection, aliases_to_find, attributes_with_alias, dep_join, false);

    // every attribute passed from "d" must have an equi predicate
    for(const auto& d_arg: attributes_with_alias){
        bool found_equi_predicate = false;
        for(auto node_with_predicates: nodes_with_predicates){
            std::shared_ptr<Ra__Node> predicate;
            switch(node_with_predicates->node_case){
                case RA__NODE__SELECTION:{
                    auto selection = std::static_pointer_cast<Ra__Node__Selection>(node_with_predicates);
                    predicate = selection->predicate;
                    break;
                }
                case RA__NODE__JOIN:{
                    auto join = std::static_pointer_cast<Ra__Node__Join>(node_with_predicates);
                    predicate = join->predicate;
                    break;
                }
            }
            if(predicate!=nullptr && has_equivalent_attribute(d_arg, predicate, d_rename_map)){
                found_equi_predicate = true;
                break;
            }
        }
        if(!found_equi_predicate){
            std::cout << std::static_pointer_cast<Ra__Node__Attribute>(d_arg)->name << std::endl;
            return false;
        }
    }
    return true;
}

void RaTree::push_down_dep_join(std::shared_ptr<Ra__Node__Join> dep_join, std::shared_ptr<Ra__Node> original_right_projection, std::queue<std::shared_ptr<Ra__Node__Join>>& push_down_dep_joins_queue){
    
    std::shared_ptr<Ra__Node> dep_join_parent = original_right_projection;
    int dep_join_parent_child_id;
    assert(get_node_parent(dep_join_parent, dep_join, dep_join_parent_child_id));
    
    // // switch dep join parent node case
    // int dep_join_parent_child_id = -1;
    // for(int i=0; i<dep_join_parent->childNodes.size(); i++){
    //     if(dep_join_parent->childNodes[i]->node_case==RA__NODE__JOIN &&
    //         std::static_pointer_cast<Ra__Node__Join>(dep_join_parent->childNodes[i])->type==RA__JOIN__DEPENDENT_INNER_LEFT){
    //         dep_join_parent_child_id = i;
    //         break;
    //     }
    // }
    assert(dep_join_parent_child_id!=-1);
    // std::shared_ptr<Ra__Node> dep_join = dep_join_parent->childNodes[dep_join_parent_child_id];

    // switch dep join right child node case
    switch(dep_join->childNodes[1]->node_case){
        // single child cases
        case RA__NODE__PROJECTION:
        case RA__NODE__SELECTION:{
            std::shared_ptr<Ra__Node> dep_join_right_child = dep_join->childNodes[1];

            dep_join->childNodes[1] = dep_join_right_child->childNodes[0];
            dep_join_right_child->childNodes[0] = dep_join;
            dep_join_parent->childNodes[dep_join_parent_child_id] = dep_join_right_child;
            break;
        }
        case RA__NODE__GROUP_BY:{
            std::shared_ptr<Ra__Node> dep_join_right_child = dep_join->childNodes[1];

            // get attributes produced by d
            std::vector<std::string> d_attributes = std::static_pointer_cast<Ra__Node__Projection>(dep_join->childNodes[0])->subquery_columns;
            for(const auto& d_attr: d_attributes){
                std::static_pointer_cast<Ra__Node__Group_By>(dep_join_right_child)->args.push_back(std::make_shared<Ra__Node__Attribute>(d_attr, "d"));
            }
            std::static_pointer_cast<Ra__Node__Group_By>(dep_join_right_child)->implicit = false;

            dep_join->childNodes[1] = dep_join_right_child->childNodes[0];
            dep_join_right_child->childNodes[0] = dep_join;
            dep_join_parent->childNodes[dep_join_parent_child_id] = dep_join_right_child;
            break;
        }
        case RA__NODE__JOIN:{
            auto child_join = std::static_pointer_cast<Ra__Node__Join>(dep_join->childNodes[1]);
            switch(child_join->type){
                case RA__JOIN__CROSS_PRODUCT:
                case RA__JOIN__INNER:{
                    if(intersect_correlated_attributes(dep_join->childNodes[0],dep_join->childNodes[1]->childNodes[1]).empty()){
                        dep_join->childNodes[1] = child_join->childNodes[0];
                        child_join->childNodes[0] = dep_join;
                        dep_join_parent->childNodes[dep_join_parent_child_id] = child_join;
                    }
                    else if(intersect_correlated_attributes(dep_join->childNodes[0],dep_join->childNodes[1]->childNodes[0]).empty()){
                        dep_join->childNodes[1] = child_join->childNodes[1];
                        child_join->childNodes[1] = dep_join;
                        dep_join_parent->childNodes[dep_join_parent_child_id] = child_join;
                    }
                    else{
                        // 1. add projection to left and right side, with aliases
                        std::shared_ptr<Ra__Node__Projection> left_pr = std::make_shared<Ra__Node__Projection>();
                        std::shared_ptr<Ra__Node__Projection> right_pr = std::make_shared<Ra__Node__Projection>();
                        std::shared_ptr<Ra__Node__Join> dep_join_right = std::make_shared<Ra__Node__Join>(RA__JOIN__DEPENDENT_INNER_LEFT);

                        // push original dep join down left side 
                        left_pr->subquery_alias = "temp_" + std::to_string(counter++);
                        left_pr->childNodes.push_back(dep_join_parent->childNodes[dep_join_parent_child_id]);
                        dep_join_parent->childNodes[dep_join_parent_child_id]->childNodes[1] = child_join->childNodes[0];
                        dep_join_parent->childNodes[dep_join_parent_child_id] = child_join; // skip dep join

                        
                        right_pr->subquery_alias = "temp_" + std::to_string(counter++);
                        right_pr->childNodes.push_back(dep_join_right);
                        dep_join_right->childNodes.push_back(dep_join->childNodes[0]); // D
                        dep_join_right->childNodes.push_back(child_join->childNodes[1]);
                        
                        child_join->childNodes[0] = left_pr;
                        child_join->childNodes[1] = right_pr;

                        // add select expressions to left projection
                        std::vector<std::pair<std::string,std::string>> left_relations_aliases;
                        get_relations_aliases(left_pr->childNodes[0], left_relations_aliases);
                        std::vector<std::string> left_aliases;
                        for(const auto& relation_alias: left_relations_aliases){
                            // relation has alias
                            if(relation_alias.second!=""){
                                left_aliases.push_back(relation_alias.second);
                            }
                            // relation has no alias
                            else{
                                left_aliases.push_back(relation_alias.first);
                            }
                        }
                        std::vector<std::shared_ptr<Ra__Node>> left_attributes;
                        find_attributes_using_alias(original_right_projection, left_aliases, left_attributes, child_join, true);
                        std::vector<std::string> temp_select_attr_tracker;
                        for(auto attribute: left_attributes){
                            auto attr = std::static_pointer_cast<Ra__Node__Attribute>(attribute);
                            if(attr->alias=="d"){
                                continue;
                            }
                            if(std::find(temp_select_attr_tracker.begin(),temp_select_attr_tracker.end(),attr->alias+attr->name)==temp_select_attr_tracker.end()){
                                temp_select_attr_tracker.push_back(attr->alias+attr->name);
                                left_pr->args.push_back(std::make_shared<Ra__Node__Select_Expression>(std::make_shared<Ra__Node__Attribute>(attr->name, attr->alias)));
                                left_pr->subquery_columns.push_back(attr->name);
                            }
                        }
                        // add d_attributes separately
                        std::vector<std::string> aliases_to_find = {"d"};
                        std::vector<std::shared_ptr<Ra__Node>> left_attributes_with_alias;
                        find_attributes_using_alias(dep_join->childNodes[1], aliases_to_find, left_attributes_with_alias);
                        for(auto attribute: left_attributes_with_alias){
                            auto attr = std::static_pointer_cast<Ra__Node__Attribute>(attribute);
                            if(std::find(temp_select_attr_tracker.begin(),temp_select_attr_tracker.end(),attr->alias+attr->name)==temp_select_attr_tracker.end()){
                                temp_select_attr_tracker.push_back(attr->alias+attr->name);
                                left_pr->args.push_back(std::make_shared<Ra__Node__Select_Expression>(std::make_shared<Ra__Node__Attribute>(attr->name, attr->alias)));
                                left_pr->subquery_columns.push_back(attr->name);
                            }
                        }

                        // add select expressions to right projection
                        std::vector<std::pair<std::string,std::string>> right_relations_aliases;
                        get_relations_aliases(right_pr->childNodes[0], right_relations_aliases);
                        std::vector<std::string> right_aliases;
                        for(const auto& relation_alias: right_relations_aliases){
                            // relation has alias
                            if(relation_alias.second!=""){
                                right_aliases.push_back(relation_alias.second);
                            }
                            // relation has no alias
                            else{
                                right_aliases.push_back(relation_alias.first);
                            }
                        }
                        std::vector<std::shared_ptr<Ra__Node>> right_attributes;
                        find_attributes_using_alias(original_right_projection, right_aliases, right_attributes, child_join, true);
                        temp_select_attr_tracker.clear();
                        for(auto attribute: right_attributes){
                            auto attr = std::static_pointer_cast<Ra__Node__Attribute>(attribute);
                            if(attr->alias=="d"){
                                continue;
                            }
                            if(std::find(temp_select_attr_tracker.begin(),temp_select_attr_tracker.end(),attr->alias+attr->name)==temp_select_attr_tracker.end()){
                                temp_select_attr_tracker.push_back(attr->alias+attr->name);
                                right_pr->args.push_back(std::make_shared<Ra__Node__Select_Expression>(std::make_shared<Ra__Node__Attribute>(attr->name, attr->alias)));
                                right_pr->subquery_columns.push_back(attr->name);
                            }
                        }
                        // add d_attributes separately
                        std::vector<std::shared_ptr<Ra__Node>> right_attributes_with_alias;
                        find_attributes_using_alias(dep_join_right->childNodes[1], aliases_to_find, right_attributes_with_alias);
                        for(auto attribute: right_attributes_with_alias){
                            auto attr = std::static_pointer_cast<Ra__Node__Attribute>(attribute);
                            if(std::find(temp_select_attr_tracker.begin(),temp_select_attr_tracker.end(),attr->alias+attr->name)==temp_select_attr_tracker.end()){
                                temp_select_attr_tracker.push_back(attr->alias+attr->name);
                                right_pr->args.push_back(std::make_shared<Ra__Node__Select_Expression>(std::make_shared<Ra__Node__Attribute>(attr->name, attr->alias)));
                                right_pr->subquery_columns.push_back(attr->name);
                            }
                        }

                        // 2. rename attributes in subquery to new aliases
                        // d -> either left or right alias
                        // rename relations defined in left/right side to their subquery alias
                        std::map<std::pair<std::string,std::string>, std::pair<std::string,std::string>> rename_map;
                        for(auto arg: left_pr->args){
                            auto expression = std::static_pointer_cast<Ra__Node__Select_Expression>(arg)->expression;
                            assert(expression->node_case==RA__NODE__ATTRIBUTE);
                            auto attribute = std::static_pointer_cast<Ra__Node__Attribute>(expression);
                            rename_map[{"d",attribute->name}] = {left_pr->subquery_alias,attribute->name};
                            for(auto relation_alias: left_relations_aliases){
                                // relation has alias
                                if(relation_alias.second!=""){
                                    rename_map[{relation_alias.second,attribute->name}] = {left_pr->subquery_alias,attribute->name};
                                }
                                // relation has no alias
                                else{
                                    rename_map[{relation_alias.first,attribute->name}] = {left_pr->subquery_alias,attribute->name};
                                }
                            }
                        }
                        for(auto arg: right_pr->args){
                            auto expression = std::static_pointer_cast<Ra__Node__Select_Expression>(arg)->expression;
                            assert(expression->node_case==RA__NODE__ATTRIBUTE);
                            auto attribute = std::static_pointer_cast<Ra__Node__Attribute>(expression);
                            rename_map[{"d",attribute->name}] = {right_pr->subquery_alias,attribute->name};
                            for(auto relation_alias: right_relations_aliases){
                                // relation has alias
                                if(relation_alias.second!=""){
                                    rename_map[{relation_alias.second,attribute->name}] = {right_pr->subquery_alias,attribute->name};
                                }
                                // relation has no alias
                                else{
                                    rename_map[{relation_alias.first,attribute->name}] = {right_pr->subquery_alias,attribute->name};
                                }
                            }
                        }
                        rename_attributes(original_right_projection, rename_map, child_join);

                        // 3. add natural join D predicates
                        std::vector<std::shared_ptr<Ra__Node>> natural_join_D_predicates;
                        for(const auto& right_attribute: right_attributes_with_alias){
                            for(const auto& left_attribute: left_attributes_with_alias){
                                auto r_attr = std::static_pointer_cast<Ra__Node__Attribute>(right_attribute);
                                auto l_attr = std::static_pointer_cast<Ra__Node__Attribute>(left_attribute);
                                if(r_attr->name==l_attr->name){
                                    auto left = std::make_shared<Ra__Node__Attribute>(l_attr->name, left_pr->subquery_alias);
                                    auto right = std::make_shared<Ra__Node__Attribute>(r_attr->name, right_pr->subquery_alias);
                                    natural_join_D_predicates.push_back(std::make_shared<Ra__Node__Predicate>(left, right, "="));
                                    break;
                                }
                            }
                        }
                        auto d_projection = std::static_pointer_cast<Ra__Node__Projection>(dep_join_right->childNodes[0]);
                        // assuming predicate is currently empty
                        assert(child_join->predicate==nullptr);
                        if(natural_join_D_predicates.size()>1){
                            child_join->type = RA__JOIN__INNER;
                            auto bool_p = std::make_shared<Ra__Node__Bool_Predicate>();
                            bool_p->bool_operator = RA__BOOL_OPERATOR__AND;
                            for(auto arg: natural_join_D_predicates){
                                bool_p->args.push_back(arg);
                            }
                            child_join->predicate = bool_p;
                        }
                        else if(natural_join_D_predicates.size()==1){
                            child_join->type = RA__JOIN__INNER;
                            child_join->predicate = natural_join_D_predicates[0];
                        }
                        else{
                            child_join->type = RA__JOIN__CROSS_PRODUCT;
                        }

                        push_down_dep_joins_queue.push(dep_join_right);

                        // setup so dep_join_parent will be set as left_pr at end of function
                        dep_join_parent = child_join;
                        dep_join_parent_child_id = 0;
                    }
                    break;
                }
                case RA__JOIN__LEFT:{
                    if(intersect_correlated_attributes(dep_join->childNodes[0],dep_join->childNodes[1]->childNodes[1]).empty()){
                        std::shared_ptr<Ra__Node> child_join = dep_join->childNodes[1];
                        dep_join->childNodes[1] = child_join->childNodes[0];
                        child_join->childNodes[0] = dep_join;
                        dep_join_parent->childNodes[dep_join_parent_child_id] = child_join;
                    }
                    else{

                    }
                    break;
                }
                case RA__JOIN__FULL_OUTER:{
                    break;
                }
            }
            break;
        }
    }
    dep_join_parent = dep_join_parent->childNodes[dep_join_parent_child_id];
}

// compute intersect of attributes produced by left with free attributes of right
std::vector<std::shared_ptr<Ra__Node>> RaTree::intersect_correlated_attributes(std::shared_ptr<Ra__Node> d_projection, std::shared_ptr<Ra__Node> comparsion_subtree){
    std::vector<std::shared_ptr<Ra__Node>> intersect_result; 

    // get relations of left side
    std::vector<std::pair<std::string, std::string>> left_relations_aliases;
    // if D projection has already been initialized
    auto left_projection = std::static_pointer_cast<Ra__Node__Projection>(d_projection);
    if(left_projection->args.size()>0){
        left_relations_aliases.push_back({"","d"});
    }
    else{
        get_relations_aliases(left_projection->childNodes[0], left_relations_aliases);
    }

    // get relations of right side
    std::shared_ptr<Ra__Node> dep_right_child = comparsion_subtree;
    std::vector<std::pair<std::string, std::string>> right_relations_aliases;
    get_relations_aliases(dep_right_child, right_relations_aliases);

    // get attributes of right side
    std::vector<std::shared_ptr<Ra__Node>> right_attributes;
    get_subtree_attributes(dep_right_child, right_attributes);

    // find attributes on right side which are not covered by relations on right side
    std::vector<std::shared_ptr<Ra__Node>> right_attributes_correlated;
    for(const auto& attribute: right_attributes){
        auto attr = std::static_pointer_cast<Ra__Node__Attribute>(attribute);
        bool found_relation = false;
        for(const auto& relation: right_relations_aliases){
            if(attr->alias.length()>0 && (attr->alias==relation.first || attr->alias==relation.second)){
                found_relation = true;
                break;
            }
            else if(attr->alias=="" && is_tpch_attribute(attr->name, relation.first)){
                found_relation = true;
                break;
            }
        }
        if(!found_relation){
            right_attributes_correlated.push_back(attr);
        }
    }

    // for each right attribute, if it matches a left relation, return it
    for(const auto& attribute: right_attributes_correlated){
        auto attr = std::static_pointer_cast<Ra__Node__Attribute>(attribute);
        for(const auto& relation: left_relations_aliases){
            if(attr->alias.length()>0 && (attr->alias==relation.first || attr->alias==relation.second)){
                intersect_result.push_back(attr);
                break;
            }
            else if(attr->alias=="" && is_tpch_attribute(attr->name, relation.first)){
                intersect_result.push_back(attr);
                break;
            }
        }
    }

    return intersect_result;
}

void RaTree::get_expression_attributes(std::shared_ptr<Ra__Node> expression, std::vector<std::shared_ptr<Ra__Node>>& attributes){
    switch(expression->node_case){
        case RA__NODE__ATTRIBUTE:{
            auto attr = std::static_pointer_cast<Ra__Node__Attribute>(expression);
            attributes.push_back(attr);
            break;
        }
        case RA__NODE__EXPRESSION:{
            auto expr = std::static_pointer_cast<Ra__Node__Expression>(expression);
            if(expr->l_arg!=nullptr){
                get_expression_attributes(expr->l_arg, attributes);
            }
            if(expr->r_arg!=nullptr){
                get_expression_attributes(expr->r_arg, attributes);
            }
            break;
        }
        case RA__NODE__FUNC_CALL:{
            auto func_call = std::static_pointer_cast<Ra__Node__Func_Call>(expression);
            for(const auto& arg: func_call->args){
                get_expression_attributes(arg, attributes);
            }
            break;
        }
        case RA__NODE__TYPE_CAST:{
            auto type_cast = std::static_pointer_cast<Ra__Node__Type_Cast>(expression);
            get_expression_attributes(type_cast->expression, attributes);
            break;
        }
    }
}

void RaTree::get_predicate_attributes(std::shared_ptr<Ra__Node> predicate, std::vector<std::shared_ptr<Ra__Node>>& attributes){
    switch(predicate->node_case){
        case RA__NODE__BOOL_PREDICATE:{
            auto bool_p = std::static_pointer_cast<Ra__Node__Bool_Predicate>(predicate);
            for(const auto& p: bool_p->args){
                get_predicate_attributes(p, attributes);
            }
            break;
        }
        case RA__NODE__PREDICATE:{
            auto p = std::static_pointer_cast<Ra__Node__Predicate>(predicate);
            get_expression_attributes(p->left, attributes);
            get_expression_attributes(p->right, attributes);
            break;
        }
        case RA__NODE__WHERE_SUBQUERY_MARKER:{
            std::vector<std::pair<std::shared_ptr<Ra__Node>, std::shared_ptr<Ra__Node>>> marker_join = {{predicate,nullptr}};
            find_joins_by_markers(root, marker_join);
            get_subtree_attributes(marker_join[0].second, attributes);
            break;
        }
        default: std::cout << "Node case should not be in predicate" << std::endl;
    }
}

// get attributes used in selection
void RaTree::get_subtree_attributes(std::shared_ptr<Ra__Node> it, std::vector<std::shared_ptr<Ra__Node>>& attributes){
    switch(it->node_case){
        case RA__NODE__SELECTION:{
            auto sel = std::static_pointer_cast<Ra__Node__Selection>(it);
            get_predicate_attributes(sel->predicate, attributes);
            break;
        }
        case RA__NODE__PROJECTION:{
            auto proj = std::static_pointer_cast<Ra__Node__Projection>(it);
            for(const auto& arg: proj->args){
                get_expression_attributes(arg, attributes);
            }
            break;
        }
        case RA__NODE__JOIN:{
            auto join = std::static_pointer_cast<Ra__Node__Join>(it);
            if(join->predicate!=nullptr){
                get_predicate_attributes(join->predicate, attributes);
            }
            break;
        }
        case RA__NODE__GROUP_BY:{
            auto group_by = std::static_pointer_cast<Ra__Node__Group_By>(it);
            for(const auto& arg: group_by->args){
                get_expression_attributes(arg, attributes);
            }
            break;
        }
        // TODO: all other nodes...
    }
    
    for(const auto& child: it->childNodes){
        get_subtree_attributes(child, attributes);
    }
    return;
}

// 1. find all "exists" markers
// 2. find all "exists" joins belonging to markers
// 3. for each "exists", check if subquery simply correlated, decorrelate:
    // remove correlating predicate from subquery selection
    // set subquery select target to inner predicate attribute
    // change "exists" join to "in" join
        // set predicate attr to "in" probe (join->predicate->left)
    // change selection marker type from "exists" to "in"
void RaTree::decorrelate_all_exists(){
    // find marker in selection
    std::vector<std::pair<std::shared_ptr<Ra__Node>, std::shared_ptr<Ra__Node>>> markers_joins; // markers, joins
    
    // 1.
    find_subquery_markers(root, markers_joins, RA__JOIN__SEMI_LEFT_DEPENDENT);
    find_subquery_markers(root, markers_joins, RA__JOIN__ANTI_LEFT_DEPENDENT);

    // 2.
    find_joins_by_markers(root, markers_joins);

    // 3. loop in reverse, to decorrelate most nested exists first
    for(int i=markers_joins.size()-1; i>=0; i--){
        // assert each marker found corresponding join
        assert(markers_joins[i].second!=nullptr);
        decorrelate_exists_subquery(markers_joins[i]);
    };
}

void RaTree::find_subquery_markers(std::shared_ptr<Ra__Node> it, std::vector<std::pair<std::shared_ptr<Ra__Node>, std::shared_ptr<Ra__Node>>>& markers_joins, Ra__Join__JoinType marker_type){

    if(it->node_case==RA__NODE__SELECTION){
        auto sel = std::static_pointer_cast<Ra__Node__Selection>(it);
        find_subquery_markers(sel->predicate, markers_joins, marker_type);
    }

    else if(it->node_case==RA__NODE__BOOL_PREDICATE){
        auto bool_p = std::static_pointer_cast<Ra__Node__Bool_Predicate>(it);
        for(const auto& arg: bool_p->args){
            find_subquery_markers(arg, markers_joins, marker_type);
        }
        return;
    }

    else if(it->node_case==RA__NODE__PREDICATE){
        auto p = std::static_pointer_cast<Ra__Node__Predicate>(it);
        find_subquery_markers(p->left, markers_joins, marker_type);
        find_subquery_markers(p->right, markers_joins, marker_type);
        return;
    }

    else if(it->node_case==RA__NODE__WHERE_SUBQUERY_MARKER){
        auto marker_ = std::static_pointer_cast<Ra__Node__Where_Subquery_Marker>(it);
        if(marker_->type==marker_type){
            // add to markers
            markers_joins.push_back({it,nullptr});
        }
        return;
    }
    for(const auto& child: it->childNodes){
        find_subquery_markers(child, markers_joins, marker_type);
    }
    return;
}

void RaTree::find_joins_by_markers(std::shared_ptr<Ra__Node> it, std::vector<std::pair<std::shared_ptr<Ra__Node>, std::shared_ptr<Ra__Node>>>& markers_joins){

    if(it->n_children==0){
        return;
    }
    
    if(it->node_case==RA__NODE__JOIN){
        auto join = std::static_pointer_cast<Ra__Node__Join>(it);
        for(auto& pair: markers_joins){
            auto marker = std::static_pointer_cast<Ra__Node__Where_Subquery_Marker>(pair.first);
            if(join->right_where_subquery_marker==marker){
                assert(join->type==marker->type);
                assert(pair.second==nullptr);
                pair.second = it;
                break;
            }
        }
    }

    for(const auto& child: it->childNodes){
        find_joins_by_markers(child, markers_joins);
    }
    return;
}

void RaTree::get_relations_aliases(std::shared_ptr<Ra__Node> it, std::vector<std::pair<std::string,std::string>>& relations_aliases){
    if(it->node_case==RA__NODE__JOIN){
        auto join = std::static_pointer_cast<Ra__Node__Join>(it);

        // join with rename, only join alias visible in namespace
        if(join->alias.length()>0){
            relations_aliases.push_back({"",join->alias});
        }
        // right side is selection subquery, only recurse on left
        else if(join->right_where_subquery_marker->marker!=0){
            relations_aliases.push_back({"","marker_"+std::to_string(join->right_where_subquery_marker->marker)});
            get_relations_aliases(it->childNodes[0], relations_aliases);
        }
        else{
            get_relations_aliases(it->childNodes[0], relations_aliases);
            get_relations_aliases(it->childNodes[1], relations_aliases);
        }
        return;
    } 

    if(it->node_case==RA__NODE__RELATION){
        auto rel = std::static_pointer_cast<Ra__Node__Relation>(it);
        relations_aliases.push_back({rel->name,rel->alias});
        return;
    }

    // from subquery
    if(it->node_case==RA__NODE__PROJECTION){
        auto pr = std::static_pointer_cast<Ra__Node__Projection>(it);
        if(pr->subquery_alias!=""){
            relations_aliases.push_back({"",pr->subquery_alias});
            return;
        }
    }

    for(const auto& child: it->childNodes){
        get_relations_aliases(child, relations_aliases);
    }
    return;
}

bool RaTree::get_node_parent(std::shared_ptr<Ra__Node>& it, std::shared_ptr<Ra__Node> child_target, int& child_index){
    auto childNodes = it->childNodes;
    // for(const auto& child: childNodes){
    for(int i=0; i<childNodes.size(); i++){
        if(childNodes[i]==child_target){
            child_index = i;
            return true;
        }
    }

    bool found = false;
    for(const auto& child: childNodes){
        if(!found){
            it = child;
            found = get_node_parent(it, child_target, child_index);
        }
    }
    return found;
}

// bool RaTree::get_selection_parent(Ra__Node** it){
//     auto childNodes = (*it)->childNodes;
//     for(const auto& child: childNodes){
//         if(child->node_case==RA__NODE__SELECTION){
//             return true;
//         }
//     }

//     bool found = false;
//     for(const auto& child: childNodes){
//         if(!found){
//             *it = child;
//             found = get_selection_parent(it);
//         }
//     }
//     return found;
// }

void RaTree::get_all_selections(std::shared_ptr<Ra__Node> it, std::vector<std::shared_ptr<Ra__Node>>& selections){

    if(it->node_case==RA__NODE__SELECTION){
        selections.push_back(it);
    }

    for(const auto& child: it->childNodes){
        get_all_selections(child, selections);
    }
}

bool RaTree::get_first_selection(std::shared_ptr<Ra__Node>& it){

    if(it->node_case==RA__NODE__SELECTION){
        return true;
    }

    bool found = false;
    auto childNodes = it->childNodes;
    for(const auto& child: childNodes){
        if(!found){
            it = child;
            found = get_first_selection(it);
        }
    }
    return found;
}

bool RaTree::is_tpch_attribute(std::string attr_name, std::string relation){
    std::string::size_type pos = attr_name.find('_');
    std::string attr_prefix = attr_name.substr(0, pos);

    if(attr_prefix=="p" && relation=="part"
        || attr_prefix=="s" && relation=="supplier"
        || attr_prefix=="ps" && relation=="partsupp"
        || attr_prefix=="c" && relation=="customer"
        || attr_prefix=="n" && relation=="nation"
        || attr_prefix=="l" && relation=="lineitem"
        || attr_prefix=="r" && relation=="region"
        || attr_prefix=="o" && relation=="orders")
    {
        return true;
    }
    return false;
}

// return {outer attr, inner attr, operator}
std::tuple<std::shared_ptr<Ra__Node>,std::shared_ptr<Ra__Node>,std::string> RaTree::is_correlating_predicate(std::shared_ptr<Ra__Node__Predicate> p, const std::vector<std::pair<std::string,std::string>>& relations_aliases){
    if(p->left->node_case!=RA__NODE__ATTRIBUTE || p->right->node_case!=RA__NODE__ATTRIBUTE){
        // if either side is an const -> not correlated
        // only supporting simple correlated predicate (<attr><op><attr>), no expression with attribute
        return {nullptr,nullptr,""};
    }

    // check left and right to find out if is correlating predicate
    bool left_found_relation = false;
    auto left_attr = std::static_pointer_cast<Ra__Node__Attribute>(p->left);
    for(const auto& relation_alias: relations_aliases){
        // if alias exists, check if same as relation name or relation alias
        if(left_attr->alias.length()>0 && (left_attr->alias==relation_alias.first || left_attr->alias==relation_alias.second)){
            left_found_relation = true;
            break;
        }
        // attribute and relation no alias -> check if is tpch attribute of relation
        else if(left_attr->alias.length()==0 && relation_alias.second.length()==0 && is_tpch_attribute(left_attr->name,relation_alias.first)){
            left_found_relation = true;
            break;
        }
    }

    bool right_found_relation = false;
    auto right_attr = std::static_pointer_cast<Ra__Node__Attribute>(p->right);
    for(const auto& relation_alias: relations_aliases){
        // if alias exists, check if same as relation name or relation alias
        if(right_attr->alias.length()>0 && (right_attr->alias==relation_alias.first || right_attr->alias==relation_alias.second)){
            right_found_relation = true;
            break;
        }
        // attribute and relation no alias -> check if is tpch attribute of relation
        else if(right_attr->alias.length()==0 && relation_alias.second.length()==0 && is_tpch_attribute(right_attr->name,relation_alias.first)){
            right_found_relation = true;
            break;
        }
    }

    // not correlating predicate
    if(right_found_relation && left_found_relation){
        return {nullptr, nullptr,""};
    }
    // both side of predicate are correlating (outer) attributes, not supported
    else if(!right_found_relation && !left_found_relation){
        std::cout << "predicates with both sides correlating is not supported" << std::endl;
        return {nullptr, nullptr,""};
    }
    // return {correlating attr, non-correlating attr}
    else if(!right_found_relation){
        return {p->right, p->left, p->binaryOperator};
    }
    else{ // !left_found_relation
        return {p->left, p->right, p->binaryOperator};
    }
}

void RaTree::decorrelate_exists_trivial(bool is_boolean_predicate, std::tuple<std::shared_ptr<Ra__Node>,std::shared_ptr<Ra__Node>,std::string,size_t> correlating_predicate, std::shared_ptr<Ra__Node__Selection> sel, std::pair<std::shared_ptr<Ra__Node>, std::shared_ptr<Ra__Node>> markers_joins){

    // 2. remove correlating predicate from subquery selection
    // if single predicate, remove selection node
    if(!is_boolean_predicate){
        std::shared_ptr<Ra__Node> it = (markers_joins.second)->childNodes[1];
        // get_selection_parent(&it);
        int child_index = -1;
        get_node_parent(it, sel, child_index);
        assert(it->n_children==1 && child_index==0); // parent should have single child
        it->childNodes[child_index] = sel->childNodes[0];
        sel->childNodes.pop_back();
    }
    // if part of boolean predicate
    else{
        auto bool_p = std::static_pointer_cast<Ra__Node__Bool_Predicate>(sel->predicate);
        assert(bool_p->args.size()>1);
        bool_p->args.erase(bool_p->args.begin() + std::get<3>(correlating_predicate));
        // if only 2 predicates, convert boolean to single normal predicate
        if(bool_p->args.size()==2){
            sel->predicate = bool_p->args[0];
            bool_p->args.pop_back();
        }
    }

    // 3. set subquery select target to inner predicate attribute
    auto pr = std::static_pointer_cast<Ra__Node__Projection>((markers_joins.second)->childNodes[1]);
    assert(pr->args.size()==1);
    pr->args[0] = std::get<1>(correlating_predicate);

    // check if "exists" or "not exists"
    auto join = std::static_pointer_cast<Ra__Node__Join>(markers_joins.second);
    Ra__Join__JoinType newType;
    if(join->type==RA__JOIN__SEMI_LEFT_DEPENDENT){
        newType = RA__JOIN__IN_LEFT;
    }
    else if(join->type==RA__JOIN__ANTI_LEFT_DEPENDENT){
        newType = RA__JOIN__ANTI_IN_LEFT;
    }
    else{
        std::cout << "this should be unreachable :)" << std::endl;
        return;
    }

    // 4. change "exists"/"not exists" join to "in"/"not in" join
    join->type = newType;
    // set predicate attr to "in" probe (join->predicate->left)
    auto p = std::make_shared<Ra__Node__Predicate>();
    p->left = std::get<0>(correlating_predicate);
    join->predicate = p;
    
    // 5. change selection marker type from "exists" to "in"
    auto marker = std::static_pointer_cast<Ra__Node__Where_Subquery_Marker>(markers_joins.first);
    marker->type = newType;
}

void RaTree::get_correlating_predicates(std::shared_ptr<Ra__Node> predicate, std::vector<std::tuple<std::shared_ptr<Ra__Node>,std::shared_ptr<Ra__Node>,std::string,size_t>>& correlating_predicates, bool& is_boolean_predicate, std::vector<std::pair<std::string,std::string>> relations_aliases){
    switch(predicate->node_case){
        case RA__NODE__BOOL_PREDICATE:{
            is_boolean_predicate = true;
            auto bool_p = std::static_pointer_cast<Ra__Node__Bool_Predicate>(predicate);
                for(const auto& p: bool_p->args){
                    get_correlating_predicates(p, correlating_predicates, is_boolean_predicate, relations_aliases);
                }
            break;
        }
        case RA__NODE__PREDICATE:{
            auto p = std::static_pointer_cast<Ra__Node__Predicate>(predicate);
            std::tuple<std::shared_ptr<Ra__Node>,std::shared_ptr<Ra__Node>,std::string> cor_predicate = is_correlating_predicate(p, relations_aliases);
            if(std::get<0>(cor_predicate)!=nullptr){
                correlating_predicates.push_back(std::make_tuple(std::get<0>(cor_predicate),std::get<1>(cor_predicate),std::get<2>(cor_predicate),0));
            }
            break;
        }
        case RA__NODE__WHERE_SUBQUERY_MARKER:{
            break;
        }
        default: std::cout << "This node case should not be in selection predicate" << std::endl;
    }
}

// exists predicates must be simple <col> <operator> <col>
void RaTree::decorrelate_exists_subquery(std::pair<std::shared_ptr<Ra__Node>, std::shared_ptr<Ra__Node>> markers_joins){
    // 1. check if subquery simply correlated

    // go through subquery tree, find relation aliases/names
    std::vector<std::pair<std::string,std::string>> relations_aliases;
    std::shared_ptr<Ra__Node> it = (markers_joins.second)->childNodes[1]->childNodes[0];
    // start searching from child of subquery projection
    get_relations_aliases(it, relations_aliases);

    // find (first & only) selection in correlated subquery
    it = (markers_joins.second)->childNodes[1]->childNodes[0];
    // correlated exists must have selection
    assert(get_first_selection(it));
    auto sel = std::static_pointer_cast<Ra__Node__Selection>(it);

    std::vector<std::tuple<std::shared_ptr<Ra__Node>,std::shared_ptr<Ra__Node>,std::string,size_t>> correlating_predicates;
    bool is_boolean_predicate = false;
    get_correlating_predicates(sel->predicate, correlating_predicates, is_boolean_predicate, relations_aliases);
    
    if(is_trivially_correlated(correlating_predicates)){
        decorrelate_exists_trivial(is_boolean_predicate, correlating_predicates[0], sel, markers_joins);
    }
    else{
        decorrelate_exists_complex(correlating_predicates, markers_joins);
    }

    return;
}

// only supports refering to base tpch relations
std::shared_ptr<Ra__Node> RaTree::build_tpch_relation(std::string alias, std::vector<std::shared_ptr<Ra__Node__Attribute>> attr){
    
    std::string tpch_relation_name = "";

    std::string::size_type pos = attr[0]->name.find('_');
    std::string attr_prefix = attr[0]->name.substr(0, pos);

    if(attr_prefix=="p"){
        tpch_relation_name = "part";
    }
    else if(attr_prefix=="s"){
        tpch_relation_name = "supplier";
    } 
    else if(attr_prefix=="ps"){
        tpch_relation_name = "partsupp";
    }
    else if(attr_prefix=="c"){
        tpch_relation_name = "customer";
    }
    else if(attr_prefix=="n"){
        tpch_relation_name = "nation";
    }
    else if(attr_prefix=="l"){
        tpch_relation_name = "lineitem";
    }
    else if(attr_prefix=="r"){
        tpch_relation_name = "region";
    }
    else if(attr_prefix=="o"){
        tpch_relation_name = "orders";
    }
    else{
        std::cout << "correlated attribute does not belong to tpch relation" << std::endl;
        tpch_relation_name = "error";
    }

    auto r = std::make_shared<Ra__Node__Relation>(tpch_relation_name, alias);
    
    return r;
}

bool RaTree::get_join_parent(std::shared_ptr<Ra__Node>& it){
    auto childNodes = it->childNodes;
    for(const auto& child: childNodes){
        if(child->node_case==RA__NODE__JOIN || child->node_case==RA__NODE__RELATION){
            return true;
        }
    }

    bool found = false;
    for(const auto& child: childNodes){
        if(!found){
            it = child;
            found = get_join_parent(it);
        }
    }
    return found;
}

bool RaTree::get_join_marker_parent(std::shared_ptr<Ra__Node>& it, std::shared_ptr<Ra__Node__Where_Subquery_Marker> marker, int& index){
    auto childNodes = it->childNodes;
    for(size_t i=0; i<it->n_children; i++){
        if(childNodes[i]->node_case==RA__NODE__JOIN && std::static_pointer_cast<Ra__Node__Join>(childNodes[i])->right_where_subquery_marker==marker){
            index = i;
            return true;
        }
    }

    bool found = false;
    for(const auto& child: childNodes){
        if(!found){
            it = child;
            found = get_join_marker_parent(it, marker, index);
        }
    }
    return found;
}

bool RaTree::find_marker_in_predicate(std::shared_ptr<Ra__Node>& it, std::shared_ptr<Ra__Node__Where_Subquery_Marker> marker, int& child_index){

    if(it->node_case==RA__NODE__BOOL_PREDICATE){
        auto bool_p = std::static_pointer_cast<Ra__Node__Bool_Predicate>(it);
        for(size_t i=0; i<bool_p->args.size(); i++){
            if(bool_p->args[i]->node_case==RA__NODE__WHERE_SUBQUERY_MARKER && std::static_pointer_cast<Ra__Node__Where_Subquery_Marker>(bool_p->args[i])==marker){
                child_index = i;
                return true;
            }
            // if marker is part of boolean "not", then return grandparent of marker
            else if(bool_p->args[i]->node_case==RA__NODE__BOOL_PREDICATE){
                auto bool_not_p = std::static_pointer_cast<Ra__Node__Bool_Predicate>(bool_p->args[i]);
                if(bool_not_p->bool_operator==RA__BOOL_OPERATOR__NOT){
                    for(size_t j=0; j<bool_not_p->args.size(); j++){
                        if(bool_not_p->args[j]->node_case==RA__NODE__WHERE_SUBQUERY_MARKER && std::static_pointer_cast<Ra__Node__Where_Subquery_Marker>(bool_not_p->args[j])==marker){
                            child_index = i;
                            return true;
                        }
                    }
                }
            }
        }
        bool found = false;
        for(const auto& arg: bool_p->args){
            if(!found){
                it = arg;
                found = find_marker_in_predicate(it, marker, child_index);
            }
        }
        return found;
    }

    else if(it->node_case==RA__NODE__PREDICATE){
        auto p = std::static_pointer_cast<Ra__Node__Predicate>(it);

        if(p->left->node_case==RA__NODE__WHERE_SUBQUERY_MARKER && std::static_pointer_cast<Ra__Node__Where_Subquery_Marker>(p->left)==marker){
            child_index = 0;
            return true;
        }
        else if(p->right->node_case==RA__NODE__WHERE_SUBQUERY_MARKER && std::static_pointer_cast<Ra__Node__Where_Subquery_Marker>(p->right)==marker){
            child_index = 1;
            return true;
        }
        else{
            bool found = false;

            // find in left child
            it = p->left;
            found = find_marker_in_predicate(it, marker, child_index);

            // find in right child
            if(!found){
                it = p->right;
                found = find_marker_in_predicate(it, marker, child_index);
            }

            return found;
        }
    }
    return false;
}

bool RaTree::find_marker_parent(std::shared_ptr<Ra__Node>& it, std::shared_ptr<Ra__Node__Where_Subquery_Marker> marker, int& child_index){

    if(it->node_case==RA__NODE__SELECTION){
        auto sel = std::static_pointer_cast<Ra__Node__Selection>(it);
        if(sel->predicate->node_case==RA__NODE__WHERE_SUBQUERY_MARKER && std::static_pointer_cast<Ra__Node__Where_Subquery_Marker>(sel->predicate)==marker){
            return true;
        }
        // if marker is part of boolean "not", then return grandparent of marker
        else if(sel->predicate->node_case==RA__NODE__BOOL_PREDICATE){
            auto bool_p = std::static_pointer_cast<Ra__Node__Bool_Predicate>(it);
            if(bool_p->bool_operator==RA__BOOL_OPERATOR__NOT){
                for(size_t i=0; i<bool_p->args.size(); i++){
                    if(bool_p->args[i]->node_case==RA__NODE__WHERE_SUBQUERY_MARKER && std::static_pointer_cast<Ra__Node__Where_Subquery_Marker>(bool_p->args[i])==marker){
                        child_index = i;
                        return true;
                    }
                }
            }
        }
        std::shared_ptr<Ra__Node> temp_it = sel->predicate;
        if(find_marker_in_predicate(temp_it, marker, child_index)){
            it = temp_it;
            return true;
        }
    }

    auto childNodes = it->childNodes;
    bool found = false;
    for(const auto& child: childNodes){
        if(!found){
            it = child;
            found = find_marker_parent(it, marker, child_index);
        }
    }
    return found;
}

bool RaTree::find_highest_node_with_only_required_relations_defined(std::shared_ptr<Ra__Node>& it, std::set<std::string> required_relations){
    std::vector<std::pair<std::string,std::string>> defined_relations_aliases;
    get_relations_aliases(it, defined_relations_aliases);
    if(defined_relations_aliases.size()==required_relations.size()){
        for(const auto& required_relation: required_relations){
            bool found = false;
            for(const auto& defined_relation_alias: defined_relations_aliases){
                if(required_relation==defined_relation_alias.first || required_relation==defined_relation_alias.second){
                    found = true;
                    break;
                }
            }
            if(!found){
                return false;
            }
        }
        return true;
    }

    auto childNodes = it->childNodes;
    bool found = false;
    for(const auto& child: childNodes){
        if(!found){
            it = child;
            found = find_highest_node_with_only_required_relations_defined(it, required_relations);
        }
    }
    return found;
}

void RaTree::decorrelate_exists_complex(std::vector<std::tuple<std::shared_ptr<Ra__Node>,std::shared_ptr<Ra__Node>,std::string,size_t>> correlating_predicates, std::pair<std::shared_ptr<Ra__Node>, std::shared_ptr<Ra__Node>> markers_joins){
    // move subquery to cte, give alias and columns (correlating attributes)
    auto exists_join = std::static_pointer_cast<Ra__Node__Join>(markers_joins.second);
    auto cte_subquery = std::static_pointer_cast<Ra__Node__Projection>(exists_join->childNodes[1]);
    std::string cte_name = "m" + std::to_string(counter++);
    cte_subquery->subquery_alias = cte_name;

    // select distinct correlating attributes
    cte_subquery->distinct = true;
    cte_subquery->args.clear();
    for(const auto& p: correlating_predicates){
        auto attr = std::static_pointer_cast<Ra__Node__Attribute>(std::get<0>(p));
        cte_subquery->subquery_columns.push_back(attr->name);
        cte_subquery->args.push_back(attr);
    }

    // list all correlating relations (using tpch name check of correlated attributes)
    std::unordered_map<std::string, std::vector<std::shared_ptr<Ra__Node__Attribute>>> alias_attr_map; // map relation (identifier) to correlating attributes
    for(const auto& p: correlating_predicates){
        // check correlating attribute
        auto attr = std::static_pointer_cast<Ra__Node__Attribute>(std::get<0>(p));

        if(attr->alias.length()>0){
            auto it = alias_attr_map.find(attr->alias);
            if (it != alias_attr_map.end()){
                alias_attr_map[attr->alias].push_back(attr);
            }
            else{
                alias_attr_map[attr->alias] = {attr};
            }
        }
        // correlating attribute has no alias
        else{
            std::string::size_type pos = attr->name.find('_');
            std::string attr_prefix = attr->name.substr(0, pos);
            auto it = alias_attr_map.find(attr_prefix);
            if (it != alias_attr_map.end()){
                alias_attr_map[attr_prefix].push_back(attr);
            }
            else{
                alias_attr_map[attr_prefix] = {attr};
            }
        }
    }

    std::vector<std::shared_ptr<Ra__Node>> correlated_relations;
    for(const auto& alias_attr: alias_attr_map){
        correlated_relations.push_back(build_tpch_relation(alias_attr.first, alias_attr.second));
    }

    // cte subquery: add correlated relations
    std::shared_ptr<Ra__Node> it = cte_subquery;
    assert(get_join_parent(it));
    for(const auto& r: correlated_relations){
        assert(it->n_children==1);
        auto cp = std::make_shared<Ra__Node__Join>(RA__JOIN__CROSS_PRODUCT);
        cp->childNodes.push_back(it->childNodes[0]);
        cp->childNodes.push_back(r);
        it->childNodes[0] = cp;
    }
    
    ctes.push_back(cte_subquery);

    /**
     * find which relations are needed for left join
     * - check correlating_predicates args alias (if no alias, check tpch name)
     * find lowest child where needed relations are defined -> add left join there
     */
    std::set<std::string> left_join_required_relations;
    for(const auto& cor_predicate: correlating_predicates){
        auto r_attr = std::static_pointer_cast<Ra__Node__Attribute>(std::get<0>(cor_predicate));
        if(r_attr->alias!=""){
            left_join_required_relations.insert(r_attr->alias);
        }
        else{
            left_join_required_relations.insert(get_tpch_relation_name(r_attr->name));
        }
    }
    // only supporting left joining with one relation
    assert(left_join_required_relations.size()==1);
    it = root;
    assert(find_highest_node_with_only_required_relations_defined(it, left_join_required_relations));
    // left join children: {it, cte}
    std::shared_ptr<Ra__Node> insert_join_here = it;
    it = root;
    int child_index = -1;
    assert(get_node_parent(it, insert_join_here, child_index));
    auto left_join = std::make_shared<Ra__Node__Join>(RA__JOIN__LEFT); 
    auto cte_relation = std::make_shared<Ra__Node__Relation>(cte_name); 
    left_join->childNodes.push_back(it->childNodes[child_index]);
    left_join->childNodes.push_back(cte_relation);
    it->childNodes[child_index] = left_join;


    // main tree: left join cte on correlating attributes (equi join)
    // it = root;
    // assert(get_join_parent(&it));
    // Ra__Node__Join* left_join = new Ra__Node__Join(RA__JOIN__LEFT); 
    // Ra__Node__Relation* cte_relation = new Ra__Node__Relation(cte_name); 
    // left_join->childNodes.push_back(it->childNodes[0]);
    // left_join->childNodes.push_back(cte_relation);
    if(correlating_predicates.size()==1){
        auto join_p = std::make_shared<Ra__Node__Predicate>();
        auto cor_predicate = correlating_predicates[0];
        auto l_attr = std::static_pointer_cast<Ra__Node__Attribute>(std::get<1>(cor_predicate));
        auto r_attr = std::static_pointer_cast<Ra__Node__Attribute>(std::get<0>(cor_predicate));
        join_p->binaryOperator = "=";
        join_p->left = std::make_shared<Ra__Node__Attribute>(l_attr->name, cte_name);
        join_p->right = std::make_shared<Ra__Node__Attribute>(r_attr->name, r_attr->alias);
        left_join->predicate = join_p;
    }
    else{
        auto join_p = std::make_shared<Ra__Node__Bool_Predicate>();
        join_p->bool_operator = RA__BOOL_OPERATOR__AND;
        for(const auto& cor_predicate: correlating_predicates){
            auto p = std::make_shared<Ra__Node__Predicate>();
            auto l_attr = std::static_pointer_cast<Ra__Node__Attribute>(std::get<1>(cor_predicate));
            auto r_attr = std::static_pointer_cast<Ra__Node__Attribute>(std::get<0>(cor_predicate));
            p->binaryOperator = "=";
            p->left = std::make_shared<Ra__Node__Attribute>(l_attr->name, cte_name);
            p->right = std::make_shared<Ra__Node__Attribute>(r_attr->name, r_attr->alias);
            join_p->args.push_back(p);
        }
        left_join->predicate = join_p;
    }

    // in main selection: replace exists with null check
    // remove exists join
    it = root;
    int index = -1;
    assert(get_join_marker_parent(it, exists_join->right_where_subquery_marker, index));
    std::shared_ptr<Ra__Node> temp = it->childNodes[index];
    it->childNodes[index] = it->childNodes[index]->childNodes[0];
    temp->childNodes.clear();

    // delete exists marker from main selection, replace with null check
    it = root;
    auto marker = std::static_pointer_cast<Ra__Node__Where_Subquery_Marker>(markers_joins.first);
    assert(find_marker_parent(it, marker, index));
    auto null_test = std::make_shared<Ra__Node__Null_Test>();

    switch(marker->type){
        case RA__JOIN__SEMI_LEFT_DEPENDENT:{
            null_test->type = RA__NULL_TEST__IS_NOT_NULL;
            break;
        }
        case RA__JOIN__ANTI_LEFT_DEPENDENT:{
            null_test->type = RA__NULL_TEST__IS_NULL;
            break;
        }
        default: std::cout << "this join type should not be found here" << std::endl;
    }

    // can use any of the correlating predicates for null check
    null_test->arg = std::make_shared<Ra__Node__Attribute>(std::static_pointer_cast<Ra__Node__Attribute>(std::get<0>(correlating_predicates[0]))->name, cte_name);;
    
    replace_subquery_marker(it, index, null_test);
}

void RaTree::replace_subquery_marker(std::shared_ptr<Ra__Node> marker_parent, int child_index, std::shared_ptr<Ra__Node> replacement){
    switch(marker_parent->node_case){
        case RA__NODE__SELECTION:{
            auto sel = std::static_pointer_cast<Ra__Node__Selection>(marker_parent);
            sel->predicate = replacement;
            break;
        }
        case RA__NODE__BOOL_PREDICATE:{
            auto bool_p = std::static_pointer_cast<Ra__Node__Bool_Predicate>(marker_parent);
            bool_p->args[child_index] = replacement;
            break;
        }
        case RA__NODE__PREDICATE:{
            auto p = std::static_pointer_cast<Ra__Node__Predicate>(marker_parent);
            if(child_index==0){
                p->left = replacement;
            }
            else if(child_index==1){
                p->right = replacement;
            }
            else{
                std::cout << "This should be unreachable :)" << std::endl;
            }
            break;
        }
        default: std::cout << "This node type should not be parent of marker" << std::endl;
    }
}

bool RaTree::is_trivially_correlated(std::vector<std::tuple<std::shared_ptr<Ra__Node>,std::shared_ptr<Ra__Node>,std::string,size_t>> correlating_predicates){
    if(correlating_predicates.size()==1 && (std::get<2>(correlating_predicates[0])=="=" || std::get<2>(correlating_predicates[0])=="<>")){
        return true;
    }
    return false;
}