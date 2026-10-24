#pragma once

#include "core/PmxTypes.h"

#include <QAbstractTableModel>
#include <QList>
#include <QString>

namespace pmx {

class PmxDocumentController;

class PmxMaterialTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column {
        ColName = 0,
        ColDiffuseR,
        ColDiffuseG,
        ColDiffuseB,
        ColDiffuseA,
        ColSpecularR,
        ColSpecularG,
        ColSpecularB,
        ColSpecularPower,
        ColAmbientR,
        ColAmbientG,
        ColAmbientB,
        ColDrawFlags,
        ColEdgeR,
        ColEdgeG,
        ColEdgeB,
        ColEdgeA,
        ColEdgeSize,
        ColTextureIndex,
        ColSphereTextureIndex,
        ColSphereMode,
        ColSharedToonFlag,
        ColToonIndex,
        ColMemo,
        ColFaceVertexCount,
        ColumnCount
    };
    Q_ENUM(Column)

    enum CustomRoles {
        NameJpRole = Qt::UserRole + 1,
        NameEnRole,
        DiffuseColorRole,
        SpecularColorRole,
        AmbientColorRole,
        EdgeColorRole
    };
    Q_ENUM(CustomRoles)

    explicit PmxMaterialTableModel(PmxDocumentController* controller, QObject* parent = nullptr);
    ~PmxMaterialTableModel() override = default;

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
    static QVariant getMaterialFieldValue(const PmxMaterial& mat, int column);
    static void applyValueToMaterial(PmxMaterial& mat, int column, const QVariant& value);

    PmxDocumentController* m_controller = nullptr;
};

} // namespace pmx
