#pragma once

#include "core/PmxTypes.h"

#include <QAbstractTableModel>
#include <QList>
#include <QString>

namespace pmx {

class PmxDocumentController;

class PmxJointTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column {
        ColName = 0,
        ColJointType,
        ColRigidBodyA,
        ColRigidBodyB,
        ColPosX,
        ColPosY,
        ColPosZ,
        ColRotX,
        ColRotY,
        ColRotZ,
        ColPosMinX,
        ColPosMaxX,
        ColPosMinY,
        ColPosMaxY,
        ColPosMinZ,
        ColPosMaxZ,
        ColRotMinX,
        ColRotMaxX,
        ColRotMinY,
        ColRotMaxY,
        ColRotMinZ,
        ColRotMaxZ,
        ColSpringPosX,
        ColSpringPosY,
        ColSpringPosZ,
        ColSpringRotX,
        ColSpringRotY,
        ColSpringRotZ,
        ColumnCount
    };
    Q_ENUM(Column)

    enum CustomRoles {
        NameJpRole = Qt::UserRole + 1,
        NameEnRole
    };
    Q_ENUM(CustomRoles)

    explicit PmxJointTableModel(PmxDocumentController* controller, QObject* parent = nullptr);
    ~PmxJointTableModel() override = default;

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    // Batch operations across multiple rows pushed as a single undoable command
    bool applyBatchOperation(const QList<int>& rowIndices, int column, const QString& opType, double operand);

    void resetModel();

public slots:
    void onDisplayLanguageChanged();

private:
    friend class PmxBatchOperationCommand;

    static double applyMathOp(double current, const QString& op, double operand);
    static QVariant getJointFieldValue(const PmxJoint& j, int column);
    static void applyValueToJoint(PmxJoint& j, int column, const QVariant& value);

    PmxDocumentController* m_controller = nullptr;
};

} // namespace pmx
