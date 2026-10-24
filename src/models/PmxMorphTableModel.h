#pragma once

#include "core/PmxTypes.h"

#include <QAbstractTableModel>
#include <QString>

namespace pmx {

class PmxDocumentController;

class PmxMorphTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column {
        ColName = 0,
        ColPanel,
        ColMorphType,
        ColOffsetCount,
        ColumnCount
    };
    Q_ENUM(Column)

    enum CustomRoles {
        NameJpRole = Qt::UserRole + 1,
        NameEnRole,
        PanelRole,
        MorphTypeRole,
        OffsetCountRole
    };
    Q_ENUM(CustomRoles)

    explicit PmxMorphTableModel(PmxDocumentController* controller, QObject* parent = nullptr);
    ~PmxMorphTableModel() override = default;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    void resetModel();

public slots:
    void onDisplayLanguageChanged();

private:
    static int getMorphOffsetCount(const PmxMorph& m);

    PmxDocumentController* m_controller = nullptr;
};

} // namespace pmx
