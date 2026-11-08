#include "graph_forms.hpp"
#include "references.hpp"
#include <pgp/studio/reusable_flow.hpp>

#include <algorithm>
#include <set>

namespace pgp::studio::detail {
namespace {
using NodeAccess = std::function<Node&(FlowDefinition&)>;

std::string_view component(std::string_view& path) {
    const auto separator = path.find('/');
    const auto first = path.substr(0, separator);
    path = separator == std::string_view::npos ? std::string_view{} : path.substr(separator + 1);
    return first;
}
void prune_unused(FlowDefinition& flow) {
    std::set<std::string, std::less<>> reached;
    std::vector<std::string> pending{flow.entry_node};
    if (!flow.exit_node.empty()) pending.push_back(flow.exit_node);
    while (!pending.empty()) {
        auto id = std::move(pending.back());
        pending.pop_back();
        if (!reached.insert(id).second) continue;
        visit_node_links(flow.nodes.at(id), [&](const auto& target) { pending.push_back(target); });
    }
    std::erase_if(flow.nodes, [&](const auto& node) { return !reached.contains(node.first); });
}
void redirect(FlowDefinition& flow, std::string_view from, std::string_view to) {
    const auto replace = [&](std::string& target) { if (target == from) target = to; };
    replace(flow.entry_node);
    replace(flow.exit_node);
    for (auto& [id, node] : flow.nodes) {
        (void)id;
        visit_node_links(node, replace);
    }
}
bool reaches_caller(const Collection& collection, const FlowDefinition& source, std::string_view caller,
                    std::set<std::string>& visited) {
    for (const auto& [id, node] : source.nodes) {
        (void)id;
        if (node.kind != NodeKind::subflow) continue;
        if (node.subflow_id == caller) return true;
        if (visited.insert(node.subflow_id).second &&
            reaches_caller(collection, std::get<FlowDefinition>(collection.find(node.subflow_id)->data), caller, visited))
            return true;
    }
    return false;
}
std::vector<FormOption> subflow_choices(const FormBuilder& builder) {
    std::vector<FormOption> options;
    const auto& collection = builder.draft.collection();
    for (const auto& [id, definition] : collection.definitions) {
        const auto* flow = std::get_if<FlowDefinition>(&definition.data);
        if (!flow || id == builder.id || flow->scope != builder.flow.scope || !flow->on_committed.empty() || seeded_root(*flow) ||
            (flow->group && !flow->group->empty()))
            continue;
        std::set<std::string> visited;
        if (!reaches_caller(collection, *flow, builder.id, visited)) options.push_back({id, id});
    }
    return options;
}
std::string new_terminal(FlowDefinition& flow, std::string_view prefix) {
    auto id = available_node_id(flow, prefix);
    Node end{};
    end.kind = NodeKind::end;
    flow.nodes.emplace(id, std::move(end));
    return id;
}
Effect initial_effect(const DraftDocument& draft, Scope scope) {
    const auto options = effect_choices(draft, scope);
    const auto decision = std::find_if(options.begin(), options.end(), [](const auto& item) { return item.value == "record_decision"; });
    if (options.empty()) throw std::invalid_argument("No effect operation is available in this scope.");
    return new_effect(draft, scope, (decision != options.end() ? *decision : options.front()).value);
}
void insert_node(FlowDefinition& flow, std::string& link, std::string_view choice, const DraftDocument& draft) {
    if (choice.empty()) return;
    Node node{};
    const auto kind = choice.substr(0, choice.find('/'));
    auto id = available_node_id(flow, kind);
    if (kind == "dialogue") {
        node.kind = NodeKind::dialogue;
        node.title_key = id + ".title";
        node.text_key = id + ".text";
        node.choices.push_back({"continue", id + ".continue", link});
        node.default_choice = "continue";
    } else if (kind == "random") {
        node.kind = NodeKind::random;
        node.random_stream = "narrative";
        node.branches = {{"no_event", 1, new_terminal(flow, "quiet_end")}, {"incident", 1, link}};
    } else {
        node.next = link;
        if (kind == "effect") {
            node.kind = NodeKind::effect;
            node.effects.push_back(initial_effect(draft, flow.scope));
        } else if (kind == "news" || kind == "result") {
            node.kind = kind == "news" ? NodeKind::news : NodeKind::result;
            node.text_key = id + ".text";
        } else if (kind == "delay") {
            node.kind = NodeKind::delay;
            node.delay_days = 1;
        } else if (kind == "sequence") {
            node.kind = NodeKind::sequence;
            node.children.push_back(new_terminal(flow, "child_end"));
        } else if (kind == "subflow") {
            node.kind = NodeKind::subflow;
            node.subflow_id = choice.substr(kind.size() + 1);
        } else throw std::invalid_argument("Choose a supported node type.");
    }
    flow.nodes.emplace(id, std::move(node));
    link = std::move(id);
}
std::vector<FormOption> insert_choices(const FormBuilder& builder) {
    std::vector<FormOption> options{{"", "Choose a node to insert"}, {"news", "News"}, {"dialogue", "Dialogue"},
        {"result", "Result"}, {"delay", "Delay"}, {"sequence", "Sequence"}, {"random", "Weighted branches"}};
    if (!effect_choices(builder.draft, builder.flow.scope).empty())
        options.insert(options.begin() + 1, {"effect", "Effects"});
    for (const auto& target : subflow_choices(builder))
        options.push_back({"subflow/" + target.value, "Call " + target.label});
    return options;
}
template<class Rows>
std::string unique_row(const Rows& rows, std::string_view prefix) {
    std::string result(prefix);
    for (unsigned suffix = 2; std::any_of(rows.begin(), rows.end(), [&](const auto& row) { return row.id == result; }); ++suffix)
        result = std::string(prefix) + "." + std::to_string(suffix);
    return result;
}
template<class Access>
void order_fields(FormBuilder& builder, std::size_t size, std::size_t index, Access access) {
    if (index > 0)
        builder.action("earlier", "Move earlier", "Move one place earlier in the authored order.", [access, index](FlowDefinition& flow, std::string_view) {
            auto& items = access(flow);
            std::swap(items.at(index), items.at(index - 1));
        });
    if (index + 1 < size)
        builder.action("later", "Move later", "Move one place later in the authored order.", [access, index](FlowDefinition& flow, std::string_view) {
            auto& items = access(flow);
            std::swap(items.at(index), items.at(index + 1));
        });
}
void node_fields(FormBuilder& builder, const std::string& id, const Node& node, const NodeAccess& access) {
    builder.note("Node type", std::string(node_label(node.kind)));
    builder.add("id", "Node ID", id, "Renaming updates entry, exit and all incoming links in this flow.", FormControl::text,
                [id](FlowDefinition& flow, std::string_view input) {
        if (id == input) return;
        if (!valid_id(input) || flow.nodes.contains(input)) throw std::invalid_argument("Choose an unused stable node ID.");
        auto renamed = flow.nodes.extract(id);
        renamed.key() = input;
        flow.nodes.insert(std::move(renamed));
        redirect(flow, id, input);
    });
    if (!node.next.empty()) add_link_fields(builder, "next", "Continue at", node.next,
                                            [access](FlowDefinition& flow) -> std::string& { return access(flow).next; });
    if (!node.text_key.empty())
        builder.add("text", "Text key", node.text_key, "Reference a stable localized text key.", FormControl::text,
                    [access](FlowDefinition& flow, std::string_view value) { access(flow).text_key = value; });
    if(node.kind==NodeKind::dialogue || node.kind==NodeKind::news || node.kind==NodeKind::result) {
        for(std::size_t i=0;i<node.participants.size();++i) {
            const auto& p=node.participants[i];const auto prefix="participant/"+std::to_string(i)+"/";
            const auto get=[access,i](FlowDefinition& flow)->ParticipantRef& {return access(flow).participants.at(i);};
            static const std::vector<FormOption> sources{{"0","Founder"},{"1","Company hiring identity"},{"2","Named external contact"},{"3","Organization"}};
            builder.add(prefix+"source","Participant source",std::to_string(static_cast<unsigned>(p.kind)),"Use saved company profiles or explicitly authored contacts.",FormControl::choice,
                [get,access](FlowDefinition& flow,std::string_view value) {
                    auto& person=get(flow);person.kind=static_cast<ParticipantKind>(parse_count(value,0,3));
                    if(person.kind==ParticipantKind::founder) {person.id="founder";person.name_key.clear();person.role_key.clear();person.portrait.reset();}
                    else if(person.kind==ParticipantKind::employee) {person.id="leadership.ceo";person.name_key.clear();person.role_key.clear();person.portrait.reset();}
                    else {if(person.name_key.empty())person.name_key=access(flow).text_key;if(person.role_key.empty())person.role_key=access(flow).text_key;
                        if(person.kind==ParticipantKind::contact && !person.portrait)person.portrait=core::PortraitRecipe{};
                        if(person.kind==ParticipantKind::organization)person.portrait.reset();}
                },sources);
            builder.add(prefix+"id","Participant ID",p.id,"Stable identity; employee references use an existing hiring ID or leadership.ceo.",FormControl::text,[get](FlowDefinition& flow,std::string_view value){get(flow).id=value;});
            if(p.kind==ParticipantKind::contact || p.kind==ParticipantKind::organization)
                builder.add(prefix+"name","Name message",p.name_key,"Localized contact or organization name.",FormControl::text,[get](FlowDefinition& flow,std::string_view value){get(flow).name_key=value;});
            builder.add(prefix+"role","Role message",p.role_key,"Optional for company profiles; required for external participants.",FormControl::text,[get](FlowDefinition& flow,std::string_view value){get(flow).role_key=value;});
            if(p.portrait) for(std::size_t part=0;part<p.portrait->parts.size();++part)
                builder.add(prefix+"portrait/"+std::to_string(part),"Portrait layer "+std::to_string(part+1),std::to_string(p.portrait->parts[part]),"Version 1 vector part/color ID; order follows the portrait kit.",FormControl::integer,
                    [get,part](FlowDefinition& flow,std::string_view value){get(flow).portrait->parts[part]=static_cast<std::uint8_t>(parse_count(value,0,core::portrait_part_counts[part]-1));});
            builder.action(prefix+"remove","Remove participant","Remove this participant from this presentation.",[access,i](FlowDefinition& flow,std::string_view){auto& people=access(flow).participants;people.erase(people.begin()+static_cast<std::ptrdiff_t>(i));});
        }
        if(node.participants.size()<8) builder.action("add_participant","Add participant","Add an organization; choose a different source and edit its name/role as needed.",[access](FlowDefinition& flow,std::string_view) {
            auto& node=access(flow);ParticipantRef p;p.kind=ParticipantKind::organization;p.id=unique_row(node.participants,"contact");p.name_key=node.text_key;p.role_key=node.text_key;node.participants.push_back(std::move(p));
        });
    }
    if (node.kind == NodeKind::dialogue) {
        builder.add("title", "Title key", node.title_key, "Reference a stable localized title key.", FormControl::text,
                    [access](FlowDefinition& flow, std::string_view value) { access(flow).title_key = value; });
        std::vector<FormOption> options;
        for (const auto& choice : node.choices) options.push_back({choice.id, choice.id});
        builder.add("default_choice", "Default on dismissal", node.default_choice, "The existing choice used on dismissal.",
                    FormControl::choice, [access](FlowDefinition& flow, std::string_view value) { access(flow).default_choice = value; }, std::move(options));
        if (node.choices.size() < 32)
            builder.action("append", "Add dialogue choice", "Add a choice using the first choice's continuation. Edit its own link from the outline.",
                           [id, access](FlowDefinition& flow, std::string_view) {
                auto& choices = access(flow).choices;
                const auto key = unique_row(choices, "choice");
                const auto label = id.size() + key.size() + 1 <= 128 ? id + "." + key : "studio." + key;
                choices.push_back({key, label, choices.front().next});
            });
    }
    if (node.kind == NodeKind::delay)
        builder.add("days", "Simulated days", std::to_string(node.delay_days), "Work resumes at the due morning.",
                    FormControl::integer, [access](FlowDefinition& flow, std::string_view value) {
            access(flow).delay_days = parse_count(value, 1, 1'000'000);
        });
    if (node.kind == NodeKind::effect && node.effects.size() < 64)
        builder.action("append", "Add effect", "Append an effect after the existing numbered effects.",
                       [access, &draft = builder.draft](FlowDefinition& flow, std::string_view) {
            access(flow).effects.push_back(initial_effect(draft, flow.scope));
        });
    if (node.kind == NodeKind::sequence && node.children.size() < 64)
        builder.action("append", "Add sequence step", "Append a separate child path. Each child must return before the next starts.",
                       [access](FlowDefinition& flow, std::string_view) {
            auto end = new_terminal(flow, "child_end");
            access(flow).children.push_back(std::move(end));
        });
    if (node.kind == NodeKind::subflow) {
        const auto* definition = builder.draft.collection().find(node.subflow_id);
        const auto& target = std::get<FlowDefinition>(definition->data);
        if (target.group && !target.group->empty()) {
            builder.note("Reusable group", node.subflow_id + " · revision " + std::to_string(definition->revision),
                "The pinned collection selects the exact group definition. The call resumes at Continue at.");
            for (const auto& port : *target.group)
                builder.add("binding/" + port.name, (port.output ? "Output · " : "Input · ") + port.name,
                    node.group_bindings.at(port.name), "Select a caller variable with the exact port contract.",
                    FormControl::choice, [access, name = port.name](FlowDefinition& flow, std::string_view value) {
                        access(flow).group_bindings.at(name) = value;
                    }, group_variable_options(builder.draft, builder.flow.scope, &port, &builder.flow));
        } else builder.add("subflow", "Reusable flow", node.subflow_id, "Call a compatible flow in the same scope, then resume at Continue at.",
            FormControl::choice, [access](FlowDefinition& flow, std::string_view value) { access(flow).subflow_id = value; }, subflow_choices(builder));
    }
    if (node.kind == NodeKind::random) {
        std::vector<FormOption> streams{{"narrative", "Narrative"}};
        const auto mode = builder.flow.lifecycle.mode;
        if (id == builder.flow.entry_node && (mode == LifecycleMode::interval || mode == LifecycleMode::scheduled)) {
            streams.push_back({"markets", "Markets"});
            streams.push_back({"hazards", "Hazards"});
        }
        builder.add("stream", "Random stream", node.random_stream,
                    "A system incident needs a scheduled or interval root and a terminal no_event outcome.",
                    FormControl::choice, [access](FlowDefinition& flow, std::string_view value) { access(flow).random_stream = value; },
                    std::move(streams));
        if (node.branches.size() < 32)
            builder.action("append", "Add weighted outcome", "Append a weight-one outcome with an existing incident continuation.",
                           [access](FlowDefinition& flow, std::string_view) {
                auto& branches = access(flow).branches;
                const auto first = std::find_if(branches.begin(), branches.end(), [](const auto& branch) { return branch.id != "no_event"; });
                branches.push_back({unique_row(branches, "outcome"), 1, first->next});
            });
    }
    std::set<std::string> exits;
    visit_node_links(node, [&](const auto& target) { if (target != id) exits.insert(target); });
    if (node.kind == NodeKind::end)
        for (const auto& [target, terminal] : builder.flow.nodes)
            if (target != id && terminal.kind == NodeKind::end) exits.insert(target);
    if (!exits.empty()) {
        std::vector<FormOption> options{{"", "Choose the continuation to keep"}};
        for (const auto& target : exits) options.push_back({target, target});
        builder.add("remove_node", "Remove node and unused paths", {},
                    "Redirect incoming links to this continuation. Paths with no remaining references are removed together and can be restored with Undo.",
                    FormControl::choice, [id](FlowDefinition& flow, std::string_view value) {
            if (value.empty()) return;
            redirect(flow, id, value);
            flow.nodes.erase(id);
            prune_unused(flow);
        }, std::move(options));
    }
}
void choice_fields(FormBuilder& builder, const Node& node, const NodeAccess& access, std::size_t index) {
    const auto& choice = node.choices.at(index);
    builder.add("id", "Choice ID", choice.id, "Renaming preserves the dismissal default if this is its target.",
                FormControl::text, [access, index](FlowDefinition& flow, std::string_view value) {
        auto& current = access(flow);
        auto& id = current.choices.at(index).id;
        if (current.default_choice == id) current.default_choice = value;
        id = value;
    });
    builder.add("label", "Label key", choice.label_key, "Reference a stable localized choice label.", FormControl::text,
                [access, index](FlowDefinition& flow, std::string_view value) { access(flow).choices.at(index).label_key = value; });
    add_link_fields(builder, "next", "On choice", choice.next,
                    [access, index](FlowDefinition& flow) -> std::string& { return access(flow).choices.at(index).next; });
    order_fields(builder, node.choices.size(), index,
                 [access](FlowDefinition& flow) -> std::vector<Choice>& { return access(flow).choices; });
    if (node.choices.size() > 1)
        builder.action("remove", "Remove choice and unused paths", "Shared paths stay linked. If the default is removed, the first remaining choice becomes the default.",
                       [access, index](FlowDefinition& flow, std::string_view) {
            auto& current = access(flow);
            const bool was_default = current.choices.at(index).id == current.default_choice;
            current.choices.erase(current.choices.begin() + static_cast<std::ptrdiff_t>(index));
            if (was_default) current.default_choice = current.choices.front().id;
            prune_unused(flow);
        });
}
void branch_fields(FormBuilder& builder, const Node& node, const NodeAccess& access, std::size_t index) {
    const auto& branch = node.branches.at(index);
    builder.add("id", "Outcome ID", branch.id, "A system incident's quiet outcome keeps the ID no_event.", FormControl::text,
                [access, index](FlowDefinition& flow, std::string_view value) { access(flow).branches.at(index).id = value; });
    builder.add("weight", "Integer weight", std::to_string(branch.weight),
                "Selection uses this outcome's weight divided by the total weight.", FormControl::integer,
                [access, index](FlowDefinition& flow, std::string_view value) {
        access(flow).branches.at(index).weight = parse_count(value, 1, 1'000'000);
    });
    add_link_fields(builder, "next", "On outcome", branch.next,
                    [access, index](FlowDefinition& flow) -> std::string& { return access(flow).branches.at(index).next; });
    order_fields(builder, node.branches.size(), index,
                 [access](FlowDefinition& flow) -> std::vector<WeightedBranch>& { return access(flow).branches; });
    if (node.branches.size() > 2 && (node.random_stream == "narrative" || branch.id != "no_event"))
        builder.action("remove", "Remove outcome and unused paths", "Shared continuations stay linked.",
                       [access, index](FlowDefinition& flow, std::string_view) {
            auto& branches = access(flow).branches;
            branches.erase(branches.begin() + static_cast<std::ptrdiff_t>(index));
            prune_unused(flow);
        });
}
void child_fields(FormBuilder& builder, const Node& node, const NodeAccess& access, std::size_t index) {
    add_link_fields(builder, "next", "Step entry", node.children.at(index),
                    [access, index](FlowDefinition& flow) -> std::string& { return access(flow).children.at(index); });
    order_fields(builder, node.children.size(), index,
                 [access](FlowDefinition& flow) -> std::vector<std::string>& { return access(flow).children; });
    if (node.children.size() > 1)
        builder.action("remove", "Remove step and unused paths", "Shared continuations stay linked.",
                       [access, index](FlowDefinition& flow, std::string_view) {
            auto& children = access(flow).children;
            children.erase(children.begin() + static_cast<std::ptrdiff_t>(index));
            prune_unused(flow);
        });
}
void effect_fields(FormBuilder& builder, const Node& node, const NodeAccess& access, std::size_t index, std::string_view row,
                   std::string_view effect_key) {
    const auto& effect = node.effects.at(index);
    const EffectAccess target = [access, index](FlowDefinition& flow) -> Effect& { return access(flow).effects.at(index); };
    if (row != effect_key) {
        if (!effect.value_expression) throw std::invalid_argument("Choose a current effect row.");
        const auto& variable = std::get<std::string>(effect.arguments.at("variable"));
        if (!build_expression_form(builder, row, *effect.value_expression, std::string(effect_key) + "/expression",
                                   *builder.draft.registry().fact(variable),
                                   [target](FlowDefinition& flow) -> Expression& { return target(flow).value_expression.value(); }))
            throw std::invalid_argument("Choose a current expression operand.");
        return;
    }
    build_effect_fields(builder, effect, target);
    order_fields(builder, node.effects.size(), index,
                 [access](FlowDefinition& flow) -> std::vector<Effect>& { return access(flow).effects; });
    if (node.effects.size() > 1)
        builder.action("remove", "Remove this effect", "Other effects retain their relative order.",
                       [access, index](FlowDefinition& flow, std::string_view) {
            auto& effects = access(flow).effects;
            effects.erase(effects.begin() + static_cast<std::ptrdiff_t>(index));
        });
}
void append_node_outline(std::vector<OutlineRow>& rows, const FlowDefinition& flow, const std::string& id, std::set<std::string>& reached) {
    if (!reached.insert(id).second) return;
    const auto& node = flow.nodes.at(id);
    const auto key = "node/" + id;
    auto label = std::string(node_label(node.kind)) + " · " + id;
    if (id == flow.entry_node) label = "Entry · " + label;
    if (id == flow.exit_node) label = "Exit path · " + label;
    rows.push_back({key, std::move(label), 1});
    for (std::size_t index = 0; index < node.effects.size(); ++index) {
        const auto& effect = node.effects[index];
        const auto part = key + "/effect/" + std::to_string(index);
        rows.push_back({part, std::to_string(index + 1) + ". " + effect.operation + " · " + effect.target, 2});
        if (effect.value_expression) append_expression_outline(rows, *effect.value_expression, part + "/expression", 3);
    }
    for (std::size_t index = 0; index < node.choices.size(); ++index)
        rows.push_back({key + "/choice/" + std::to_string(index),
                        "Choice " + std::to_string(index + 1) + " · " + node.choices[index].id + " → " + node.choices[index].next, 2});
    for (std::size_t index = 0; index < node.children.size(); ++index)
        rows.push_back({key + "/child/" + std::to_string(index), "Step " + std::to_string(index + 1) + " → " + node.children[index], 2});
    for (std::size_t index = 0; index < node.branches.size(); ++index)
        rows.push_back({key + "/branch/" + std::to_string(index),
                        "Outcome " + std::to_string(index + 1) + " · " + node.branches[index].id + " → " + node.branches[index].next, 2});
    visit_node_links(node, [&](const auto& target) { append_node_outline(rows, flow, target, reached); });
}
} // namespace

void add_link_fields(FormBuilder& builder, std::string key, std::string label, const std::string& target, LinkAccess access) {
    builder.add(key, std::move(label), target, "Retarget this link and remove only paths no longer reachable from entry/exit. Undo restores the whole change. The compiler rejects cycles and incompatible continuations.",
                FormControl::choice, [access](FlowDefinition& flow, std::string_view value) { access(flow) = value; prune_unused(flow); },
                node_choices(builder.flow));
    builder.add(key + "/insert", "Insert on this link", {}, "Keep the current continuation after the new node or authored branch.",
                FormControl::choice, [access, &draft = builder.draft](FlowDefinition& flow, std::string_view choice) {
        insert_node(flow, access(flow), choice, draft);
    }, insert_choices(builder));
}
void append_execution_outline(std::vector<OutlineRow>& rows, const DraftDocument&, const FlowDefinition& flow) {
    rows.push_back({"execution", "Execution graph"});
    std::set<std::string> reached;
    append_node_outline(rows, flow, flow.entry_node, reached);
    if (!flow.exit_node.empty()) append_node_outline(rows, flow, flow.exit_node, reached);
}
void build_execution_form(FormBuilder& builder, std::string_view row) {
    if (row == "execution") {
        builder.note("Entry", builder.flow.entry_node);
        if (!builder.flow.exit_node.empty()) builder.note("Continuous exit", builder.flow.exit_node);
        builder.note("Nodes", std::to_string(builder.flow.nodes.size()),
                     "Select a node or numbered port from the outline. New nodes are inserted on an existing link.");
        return;
    }
    auto path = row.substr(5);
    const std::string id(component(path));
    const auto& node = builder.flow.nodes.at(id);
    const NodeAccess access = [id](FlowDefinition& flow) -> Node& { return flow.nodes.at(id); };
    if (path.empty()) {
        node_fields(builder, id, node, access);
        return;
    }
    const auto section = component(path);
    const auto index = static_cast<std::size_t>(parse_count(component(path), 0, 511));
    if (section == "effect") {
        effect_fields(builder, node, access, index, row, "node/" + id + "/effect/" + std::to_string(index));
        return;
    }
    if (!path.empty()) throw std::invalid_argument("Choose a current execution row.");
    if (section == "choice") choice_fields(builder, node, access, index);
    else if (section == "branch") branch_fields(builder, node, access, index);
    else if (section == "child") child_fields(builder, node, access, index);
    else throw std::invalid_argument("Choose a current execution row.");
}
} // namespace pgp::studio::detail
