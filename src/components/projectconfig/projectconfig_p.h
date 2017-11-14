#ifndef PROJECTCONFIG_P_H
#define PROJECTCONFIG_P_H

#include "canrawsendermodel.h"
#include "canrawviewmodel.h"
#include "flowviewwrapper.h"
#include "modeltoolbutton.h"
#include "ui_projectconfig.h"
#include <QMenu>
#include <QtWidgets/QPushButton>
#include <log.h>
#include <modelvisitor.h> // apply_model_visitor
#include <nodes/Node>
#include <projectconfig/candevicemodel.h>
#include <propertyeditor/propertyeditordialog.h>

namespace Ui {
class ProjectConfigPrivate;
}

class ProjectConfigPrivate : public QWidget {
    Q_OBJECT
    Q_DECLARE_PUBLIC(ProjectConfig)

public:
    ProjectConfigPrivate(ProjectConfig* q, QWidget* parent)
        : QWidget(parent)
        , _graphView(new FlowViewWrapper(&_graphScene))
        , _ui(std::make_unique<Ui::ProjectConfigPrivate>())
        , q_ptr(q)
    {
        auto& modelRegistry = _graphScene.registry();
        modelRegistry.registerModel<CanDeviceModel>();
        modelRegistry.registerModel<CanRawSenderModel>();
        modelRegistry.registerModel<CanRawViewModel>();

        connect(&_graphScene, &QtNodes::FlowScene::nodeCreated, this, &ProjectConfigPrivate::nodeCreatedCallback);
        connect(&_graphScene, &QtNodes::FlowScene::nodeDeleted, this, &ProjectConfigPrivate::nodeDeletedCallback);
        connect(&_graphScene, &QtNodes::FlowScene::nodeDoubleClicked, this,
            &ProjectConfigPrivate::nodeDoubleClickedCallback);
        connect(
            &_graphScene, &QtNodes::FlowScene::nodeContextMenu, this, &ProjectConfigPrivate::nodeContextMenuCallback);

        _ui->setupUi(this);
        _ui->layout->addWidget(_graphView);
    }

    ~ProjectConfigPrivate() {}

    QByteArray save() const
    {
        return _graphScene.saveToMemory();
    }

    void load(const QByteArray& data)
    {
        return _graphScene.loadFromMemory(data);
    }

    void clearGraphView()
    {
        return _graphScene.clearScene();
    };

    void nodeCreatedCallback(QtNodes::Node& node)
    {
        Q_Q(ProjectConfig);

        auto& iface = getComponentModel(node);
        iface.handleModelCreation(q);

        if (!iface.restored()) {
            iface.setCaption(node.nodeDataModel()->caption() + " #" + QString::number(_nodeCnt));
        }

        _nodeCnt++;
    }

    void nodeDeletedCallback(QtNodes::Node& node)
    {
        Q_Q(ProjectConfig);
        auto& component = getComponent(node);

        emit q->handleWidgetDeletion(component.mainWidget());
    }

    void nodeDoubleClickedCallback(QtNodes::Node& node)
    {
        auto& component = getComponent(node);

        if (component.mainWidget() != nullptr) {
            openWidget(node);
        } else {
            openProperties(node);
        }
    }

    void nodeContextMenuCallback(QtNodes::Node& node, const QPointF& pos)
    {
        auto& component = getComponent(node);
        QMenu contextMenu(tr("Node options"), this);

        QAction actionOpen("Open", this);
        connect(&actionOpen, &QAction::triggered, [this, &node]() { openWidget(node); });

        QAction actionProperties("Properties", this);
        connect(&actionProperties, &QAction::triggered, [this, &node]() { openProperties(node); });

        QAction actionDelete("Delete", this);
        connect(&actionDelete, &QAction::triggered, [this, &node]() { _graphScene.removeNode(node); });

        if (component.mainWidget() != nullptr) {
            contextMenu.addAction(&actionOpen);
            contextMenu.addAction(&actionProperties);
            contextMenu.setDefaultAction(&actionOpen);
        } else {
            contextMenu.addAction(&actionProperties);
            contextMenu.setDefaultAction(&actionProperties);
        }

        contextMenu.addAction(&actionDelete);

        auto pos1 = mapToGlobal(_graphView->mapFromScene(pos));
        pos1.setX(pos1.x() + 32); // FIXME: these values are hardcoded and should not be here
        pos1.setY(pos1.y() + 10); //        find the real cause of misalignment of context menu
        contextMenu.exec(pos1);
    }

private:
    QtNodes::FlowScene _graphScene;
    FlowViewWrapper* _graphView;
    std::unique_ptr<Ui::ProjectConfigPrivate> _ui;
    int _nodeCnt = 1;
    ProjectConfig* q_ptr;

    void openWidget(QtNodes::Node& node)
    {
        Q_Q(ProjectConfig);
        auto& component = getComponent(node);

        emit q->handleWidgetShowing(component.mainWidget(), component.mainWidgetDocked());
    }

    void openProperties(QtNodes::Node& node)
    {
        auto& component = getComponent(node);
        auto conf = component.getQConfig();
        conf->setProperty("name", node.nodeDataModel()->caption());

        PropertyEditorDialog e(node.nodeDataModel()->name() + " properties", *conf.get());
        if (e.exec() == QDialog::Accepted) {
            conf = e.properties();
            auto nodeCaption = conf->property("name");
            if (nodeCaption.isValid()) {
                auto& iface = getComponentModel(node);
                iface.setCaption(nodeCaption.toString());
                node.nodeGraphicsObject().update();
            }

            component.setConfig(*conf);
            component.configChanged();
        }
    }

    ComponentInterface& getComponent(QtNodes::Node& node)
    {
        auto &iface = getComponentModel(node);
        auto& component = iface.getComponent();
        return component;
    }

    ComponentModelInterface& getComponentModel(QtNodes::Node& node)
    {
        auto dataModel = node.nodeDataModel();
        assert(nullptr != dataModel);

        auto iface = dynamic_cast<ComponentModelInterface*>(dataModel);
        return *iface;
    }
};
#endif // PROJECTCONFIG_P_H