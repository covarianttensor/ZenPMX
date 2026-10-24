#pragma once

#include "core/PmxTypes.h"

#include <QAbstractTableModel>
#include <QList>
#include <QString>

namespace pmx {

class PmxDocumentController;

class PmxRigidBodyTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column {
        ColName = 0,
        ColBoneIndex,
        ColGroup,
        ColMask,
        ColShape,
        ColSizeX,
        ColSizeY,
        ColSizeZ,
        ColPosX,
        ColPosY,
        ColPosZ,
        ColRotX,
        ColRotY,
        ColRotZ,
        ColMass,
        ColLinearDamping,
        ColAngularDamping,
        ColRestitution,
        ColFriction,
        ColPhysicsMode,
        ColumnCount
    };
    Q_ENUM(Column)

    enum CustomRoles {
        NameJpRole = Qt::UserRole + 1,
        NameEnRole
    };
    Q_ENUM(CustomRoles)

    explicit PmxRigidBodyTableModel(PmxDocumentController* controller, QObject* parent = nullptr);
    ~PmxRigidBodyTableModel() override = default;

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
    static QVariant getRigidBodyFieldValue(const PmxRigidBody& rb, int column);
    static void applyValueToRigidBody(PmxRigidBody& rb, int column, const QVariant& value);

    PmxDocumentController* m_controller = nullptr;
};

} // namespace pmx
