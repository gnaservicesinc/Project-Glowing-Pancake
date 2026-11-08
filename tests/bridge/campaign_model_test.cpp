#include "campaign_model.hpp"

#include <QCoreApplication>
#include <QItemSelectionModel>
#include <QPersistentModelIndex>

#include <atomic>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <thread>

using namespace pgp::bridge;
namespace {
int checks = 0;
void check(bool condition,const char* explanation) {
    ++checks;
    if (!condition) throw std::runtime_error(explanation);
}
CampaignRow row(QString id,QString title,QString status = QStringLiteral("active"),bool required = false) {
    return {std::move(id),std::move(title),QStringLiteral("Work / cost"),QStringLiteral("Cause and exact 9223372036854775807 cents"),std::move(status),required};
}
void persistent_rows_and_context() {
    StableListModel model;
    int resets = 0,moves = 0,inserts = 0,removes = 0,changes = 0;
    QList<int> roles;
    QObject::connect(&model,&QAbstractItemModel::modelReset,[&] { ++resets; });
    QObject::connect(&model,&QAbstractItemModel::rowsMoved,[&] { ++moves; });
    QObject::connect(&model,&QAbstractItemModel::rowsInserted,[&] { ++inserts; });
    QObject::connect(&model,&QAbstractItemModel::rowsRemoved,[&] { ++removes; });
    QObject::connect(&model,&QAbstractItemModel::dataChanged,[&](const QModelIndex&,const QModelIndex&,const QList<int>& changed) {
        ++changes; roles = changed;
    });
    model.setRows({row(QStringLiteral("project.z"),QStringLiteral("Alpha")),
                   row(QStringLiteral("project.keep"),QStringLiteral("Zulu")),
                   row(QStringLiteral("project.old"),QStringLiteral("Middle"))});
    check(model.rowCount() == 3 && resets == 0,"First rows must insert without a routine reset");
    model.setSelectedId(QStringLiteral("project.keep"));
    model.setAnchorId(QStringLiteral("project.keep")); model.setAnchorOffset(17.5);
    QPersistentModelIndex editing(model.index(model.indexOf(QStringLiteral("project.keep"))));
    QItemSelectionModel selection(&model);
    selection.setCurrentIndex(editing,QItemSelectionModel::ClearAndSelect);
    model.setSortByTitle(true);
    check(editing.isValid() && editing.row() == 2 && selection.currentIndex() == editing,
          "Sort must move the editing/current index with its entity");
    check(model.selectedId() == QStringLiteral("project.keep") && model.anchorId() == model.selectedId() && model.anchorOffset() == 17.5,
          "Sorting must preserve stable selection and viewport offset");
    changes = 0;
    model.setRows({row(QStringLiteral("project.z"),QStringLiteral("Alpha")),
                   row(QStringLiteral("project.keep"),QStringLiteral("Aardvark")),
                   row(QStringLiteral("project.new"),QStringLiteral("Bravo"))});
    check(editing.isValid() && editing.row() == 0 && selection.currentIndex() == editing && selection.isSelected(editing),
          "Insert/remove/title-change must retain the editing entity");
    check(changes == 1 && roles == QList<int>{StableListModel::TitleRole},"Only changed role should update");
    check(model.selectedRow().value(QStringLiteral("entityId")) == QStringLiteral("project.keep") &&
          model.selectedRow().value(QStringLiteral("title")) == QStringLiteral("Aardvark"),"Inspector must follow the selected stable ID");
    model.setRows({row(QStringLiteral("project.keep"),QStringLiteral("Aardvark")),
                   row(QStringLiteral("project.z"),QStringLiteral("Alpha")),
                   row(QStringLiteral("project.new"),QStringLiteral("Bravo"))});
    check(changes == 1,"Unchanged snapshot must not emit dataChanged");

    // Two hundred successive UI snapshots exercise the same insert/remove/move
    // operations used by the capped worker stream without wall-clock sleeps.
    for (int snapshot = 0; snapshot < 200; ++snapshot) {
        auto rows = std::vector{row(QStringLiteral("project.keep"),snapshot % 2 ? QStringLiteral("Zulu") : QStringLiteral("Aardvark")),
            row(QStringLiteral("project.z"),QStringLiteral("Alpha"))};
        if (snapshot % 3 == 0) rows.push_back(row(QStringLiteral("project.transient"),QStringLiteral("Middle")));
        model.setRows(std::move(rows));
        check(editing.isValid() && editing.data(StableListModel::IdRole) == QStringLiteral("project.keep") &&
              selection.currentIndex() == editing && model.selectedId() == QStringLiteral("project.keep"),
              "Frequent snapshot diffs lost the selected/editing entity");
        check(model.anchorId() == QStringLiteral("project.keep") && model.anchorOffset() == 17.5,
              "Frequent snapshot diffs changed stable viewport context");
    }
    check(resets == 0 && moves > 100 && inserts > 20 && removes > 20,"Running snapshots must reconcile without model resets");

    const auto before = model.selectedRow(); const auto count = model.rowCount();
    bool refused = false;
    try { model.setRows({row(QStringLiteral("same"),QStringLiteral("A")),row(QStringLiteral("same"),QStringLiteral("B"))}); }
    catch (const std::invalid_argument&) { refused = true; }
    check(refused && model.rowCount() == count && model.selectedRow() == before && editing.isValid(),
          "Invalid duplicate snapshot must leave rows and context untouched");
    std::atomic<bool> denied = false;
    std::thread wrong_thread([&] {
        try { model.setRows({}); } catch (const std::logic_error&) { denied = true; }
    });
    wrong_thread.join();
    check(denied && editing.isValid(),"Worker thread cannot mutate the GUI model");

    model.setRows({row(QStringLiteral("replacement"),QStringLiteral("Replacement"))});
    check(!editing.isValid() && model.selectedId() == QStringLiteral("replacement") &&
          model.anchorId() == QStringLiteral("replacement") && model.anchorOffset() == 0,
          "Removing the context entity must choose a valid visible fallback");
    model.beginCampaign({row(QStringLiteral("18446744073709551615"),QStringLiteral("Exact ID"))});
    check(resets == 1 && model.idAt(0) == QStringLiteral("18446744073709551615") &&
          model.selectedRow().value(QStringLiteral("detail")).toString().contains(QStringLiteral("9223372036854775807")),
          "Explicit campaign replacement must reset once without integer precision loss");
}
void filtered_inbox() {
    StableListModel inbox{nullptr,true};
    int resets = 0;
    QObject::connect(&inbox,&QAbstractItemModel::modelReset,[&] { ++resets; });
    inbox.setRows({row(QStringLiteral("choice.1"),QStringLiteral("Supplier decision"),QStringLiteral("waiting"),true),
                   row(QStringLiteral("digest:2014-01-01:00000"),QStringLiteral("Weekly news digest"),QStringLiteral("read"),false),
                   row(QStringLiteral("warning.3"),QStringLiteral("Liquidity warning"),QStringLiteral("waiting"),true)});
    check(inbox.idAt(0) == QStringLiteral("choice.1") && inbox.idAt(1) == QStringLiteral("warning.3"),
          "Digest rows must sort after required choices and active deadlines");
    inbox.setSortByTitle(true);
    check(inbox.idAt(0) == QStringLiteral("warning.3") && inbox.idAt(1) == QStringLiteral("choice.1"),
          "Alphabetical sorting must retain the inbox action tier");
    inbox.setSelectedId(QStringLiteral("choice.1"));
    inbox.setAnchorId(QStringLiteral("choice.1")); inbox.setAnchorOffset(8);
    QPersistentModelIndex choice(inbox.index(inbox.indexOf(QStringLiteral("choice.1"))));
    inbox.setRequiredOnly(true);
    check(inbox.rowCount() == 2 && choice.isValid() && inbox.selectedId() == QStringLiteral("choice.1") && inbox.anchorOffset() == 8,
          "Required inbox filtering must preserve retained selection and anchor");
    inbox.setFilterText(QStringLiteral("SUPPLIER"));
    check(inbox.containsSourceId(QStringLiteral("warning.3")) && !inbox.containsSourceId(QStringLiteral("missing")),
          "Draft validation must recognize filtered source IDs and refuse unknown IDs");
    check(inbox.sourceRow(QStringLiteral("warning.3")).value(QStringLiteral("title"))==QStringLiteral("Liquidity warning") &&
          inbox.sourceRow(QStringLiteral("missing")).isEmpty() && inbox.selectedId()==QStringLiteral("choice.1"),
          "Read-only map briefings retain filtered public rows without changing selection");
    check(inbox.rowCount() == 1 && choice.isValid() && inbox.row(QStringLiteral("choice.1")).value(QStringLiteral("required")).toBool(),
          "Case-insensitive inbox filter must retain required actions");
    inbox.setRows({row(QStringLiteral("choice.1"),QStringLiteral("Supplier decision"),QStringLiteral("resolved"),false),
                   row(QStringLiteral("digest:2014-01-01:00000"),QStringLiteral("Weekly news digest"),QStringLiteral("read"),false),
                   row(QStringLiteral("warning.3"),QStringLiteral("Liquidity warning"),QStringLiteral("waiting"),true)});
    check(inbox.rowCount() == 0 && inbox.selectedId().isEmpty() && !choice.isValid(),
          "Resolved choice must leave required-only view without a model reset");
    inbox.setFilterText({}); inbox.setRequiredOnly(false);
    check(inbox.rowCount() == 3 && inbox.indexOf(QStringLiteral("warning.3")) >= 0 && resets == 0,
          "Clearing filters must recover source rows with stable diffs");
    inbox.setAnchorOffset(std::nan(""));
    check(std::isfinite(inbox.anchorOffset()),"Invalid scroll offset must not poison viewport restoration");
    check(inbox.indexOf(QStringLiteral("absent")) == -1 && inbox.row(QStringLiteral("absent")).isEmpty() && inbox.idAt(-1).isEmpty(),
          "Absent selection references must be safe presentation queries");
}
}
int main(int argc,char* argv[]) {
    QCoreApplication application(argc,argv);
    try {
        persistent_rows_and_context(); filtered_inbox();
        std::cout << checks << " campaign model checks passed\n"; return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
