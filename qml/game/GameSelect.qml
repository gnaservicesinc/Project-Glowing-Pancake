import QtQuick
import QtQuick.Controls

ComboBox {
    id: control
    property int textSize: 14
    GameInk { id: ink }
    hoverEnabled: true
    font.pixelSize: textSize
    implicitHeight: Math.max(40,font.pixelSize*2.6)
    leftPadding: 12;rightPadding: 30
    contentItem: Text {text:control.displayText;font:control.font;color:control.enabled?"#e8e5d3":"#7b9690";verticalAlignment:Text.AlignVCenter;elide:Text.ElideRight}
    background: Rectangle {radius:7;color:control.down || control.hovered?ink.hover:ink.control;border.width:control.visualFocus?2:1;border.color:control.visualFocus?ink.focus:ink.border}
    indicator: Text {x:control.width-width-12;y:(control.height-height)/2;text:"⌄";font.pixelSize:control.font.pixelSize;color:"#bed7c7"}
    delegate: ItemDelegate {
        id: row
        required property int index
        required property var model
        width: ListView.view ? ListView.view.width : control.width
        text:control.textRole && model && model[control.textRole] !== undefined ? model[control.textRole] : control.textAt(index)
        highlighted:control.highlightedIndex===index
        contentItem: Text {text:row.text;font.pixelSize:control.font.pixelSize;wrapMode:Text.Wrap;color:row.highlighted?"#fff0cb":"#d3e0d1";verticalAlignment:Text.AlignVCenter}
        background: Rectangle {color:row.highlighted?ink.hover:ink.panel}
    }
    popup: Popup {
        y:control.height+4;width:control.width;padding:5
        implicitHeight:Math.min(contentItem.implicitHeight+10,320)
        background:Rectangle {radius:8;color:ink.panel;border.color:ink.border}
        contentItem:ListView {clip:true;implicitHeight:contentHeight;model:control.popup.visible?control.delegateModel:null;currentIndex:control.highlightedIndex;ScrollIndicator.vertical:ScrollIndicator{}}
    }
}
