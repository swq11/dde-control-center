// SPDX-FileCopyrightText: 2024 - 2027 UnionTech Software Technology Co., Ltd.
// SPDX-License-Identifier: GPL-3.0-or-later
import QtQuick 2.15
import QtQuick.Controls 2.0
import QtQuick.Layouts 1.15

import org.deepin.dtk 1.0 as D
import org.deepin.dcc 1.0

Rectangle {
    id: root
    property alias model: repeater.model
    property bool backgroundVisible: true
    signal clicked(int index, bool checked)

    color: "transparent"
    implicitHeight: layoutView.height
    Layout.fillWidth: true

    ColumnLayout {
        id: layoutView
        width: parent.width
        spacing: 0
        Repeater {
            id: repeater
            model: powerModeModel
            delegate: D.ItemDelegate {
                Layout.fillWidth: true
                visible: {
                    if (model.mode === "performance") {
                        return dccData.model.isHighPerformanceSupported
                    } else if (model.mode === "balance_performance") {
                        return dccData.model.isBalancePerformanceSupported
                    }
                    return true
                }

                leftPadding: 10
                rightPadding: 10
                cascadeSelected: true
                checkable: false
                contentFlow: true
                corners: getCornersForBackground(index, powerModeModel.count)
                icon.name: model.icon
                hoverEnabled: true
                content: RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    ColumnLayout {
                        Layout.fillWidth: true
                        DccLabel {
                            Layout.fillWidth: true
                            font: D.DTK.fontManager.t6
                            text: model.title
                        }
                        DccLabel {
                            Layout.fillWidth: true
                            visible: text !== ""
                            font: D.DTK.fontManager.t10
                            text: model.description
                            opacity: 0.5
                        }
                    }
                    Control {
                        Layout.alignment: Qt.AlignRight
                        Layout.rightMargin: 10
                        contentItem: DccCheckIcon {
                            visible: model.mode === dccData.model.powerPlan
                        }
                    }
                }
                background: DccItemBackground {
                    separatorVisible: true
                    backgroundType: DccObject.ClickStyle
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        dccData.worker.setPowerPlan(model.mode)
                    }
                }
            }
        }
    }

    ListModel {
        id: powerModeModel

        ListElement {
            mode: "performance"
            title: qsTr("High Performance")
            icon: "high_performance"
            description: qsTr("Prioritize performance, which will significantly increase power consumption and heat generation")
        }

        ListElement {
            mode: "balance_performance"
            title: qsTr("Balance Performance")
            icon: "balance_performance"
            description: qsTr("Aggressively adjust CPU operating frequency based on CPU load condition")
        }

        ListElement {
            mode: "balance"
            title: qsTr("Balanced")
            icon: "balanced"
            description: qsTr("Balancing performance and battery life, automatically adjusted according to usage")
        }

        ListElement {
            mode: "powersave"
            title: qsTr("Power Saver")
            icon: "power_performance"
            description: qsTr("Prioritize battery life, which the system will sacrifice some performance to reduce power consumption")
        }
    }
}
