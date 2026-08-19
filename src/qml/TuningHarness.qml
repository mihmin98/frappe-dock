import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.frappedock

/*
 * The tuning harness: a debug window whose sliders mutate the live dock.
 *
 * This is a tool, not a settings page. It writes to the same keys the settings
 * page will (plan.md §7), so what is dialled in here is what ships — there is
 * no separate set of "harness values" to transcribe afterwards, and no chance
 * of transcribing them wrong. It deliberately offers no styling, no layout
 * polish and no persistence beyond Save: it exists to answer "does this feel
 * right", which is a question about the dock, not about this window.
 *
 * Opened only when FRAPPE_TUNING=1 is set, and only in builds configured with
 * -DFRAPPE_TUNING_HARNESS=ON.
 */
ApplicationWindow {
    id: harness

    title: qsTr("Dock geometry tuning")
    width: 460
    height: 520
    visible: true

    /// A slider with its value shown and a way back to the shipped default —
    /// without which a tuning session is one bad drag away from having lost the
    /// baseline it was being compared against.
    component Knob: ColumnLayout {
        id: knob

        required property string label
        required property real from
        required property real to
        required property real value
        required property real fallback
        /// Decimal places to show. Whole numbers for pt, two for ratios.
        property int decimals: 2

        signal moved(real value)

        spacing: 0
        Layout.fillWidth: true

        RowLayout {
            Layout.fillWidth: true

            Label {
                text: knob.label
                Layout.fillWidth: true
            }
            Label {
                text: knob.value.toFixed(knob.decimals)
                font.family: "monospace"
            }
            Button {
                text: qsTr("Reset")
                flat: true
                enabled: Math.abs(knob.value - knob.fallback) > 1e-6
                onClicked: knob.moved(knob.fallback)
            }
        }

        Slider {
            Layout.fillWidth: true
            from: knob.from
            to: knob.to
            value: knob.value
            // Bindings are one-way here on purpose: the slider proposes, the
            // config disposes. A two-way binding fights the schema's clamping
            // and leaves the handle somewhere the value is not.
            onMoved: knob.moved(value)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        Label {
            text: qsTr("Changes apply to the running dock immediately. "
                       + "Save writes them to the config file; Reload discards them.")
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        CheckBox {
            text: qsTr("Magnification")
            checked: FrappeConfig.magnificationEnabled
            onToggled: FrappeConfig.magnificationEnabled = checked
        }

        Knob {
            label: qsTr("Peak size (x tile)")
            from: 1.0
            to: 3.0
            value: FrappeConfig.magnificationFactor
            fallback: 2.0
            enabled: FrappeConfig.magnificationEnabled
            onMoved: (v) => FrappeConfig.magnificationFactor = v
        }

        Knob {
            label: qsTr("Falloff radius (cell pitches)")
            from: 1.0
            to: 8.0
            value: FrappeConfig.falloffRadius
            fallback: 3.0
            enabled: FrappeConfig.magnificationEnabled
            onMoved: (v) => FrappeConfig.falloffRadius = v
        }

        Knob {
            label: qsTr("Curve exponent")
            from: 0.5
            to: 4.0
            value: FrappeConfig.curveExponent
            fallback: 1.6
            enabled: FrappeConfig.magnificationEnabled
            onMoved: (v) => FrappeConfig.curveExponent = v
        }

        Knob {
            label: qsTr("Tile size (pt)")
            from: 24
            to: 128
            value: FrappeConfig.tileSize
            fallback: 48
            decimals: 0
            onMoved: (v) => FrappeConfig.tileSize = Math.round(v)
        }

        Knob {
            label: qsTr("Animation speed (%)")
            from: 0
            to: 400
            value: FrappeConfig.animationSpeed
            fallback: 100
            decimals: 0
            onMoved: (v) => FrappeConfig.animationSpeed = Math.round(v)
        }

        Knob {
            label: qsTr("Gap ratio (x tile)")
            from: 0.1
            to: 0.8
            value: GeometryTuning.spacingRatio
            fallback: 1 / 3
            decimals: 3
            // Not a config key and never will be: the gap is S/3 by the
            // proportion model, and the shelf's inner padding is the same
            // number. This slider exists to test that choice, not to offer it.
            onMoved: (v) => GeometryTuning.spacingRatio = v
        }

        Item { Layout.fillHeight: true }

        RowLayout {
            Layout.fillWidth: true

            Button {
                text: qsTr("Save")
                onClicked: FrappeConfig.save()
            }
            Button {
                text: qsTr("Reload")
                onClicked: {
                    FrappeConfig.reload();
                    GeometryTuning.reset();
                }
            }
            Item { Layout.fillWidth: true }
            Label {
                text: qsTr("Gap ratio is never saved.")
                opacity: 0.6
            }
        }
    }
}
