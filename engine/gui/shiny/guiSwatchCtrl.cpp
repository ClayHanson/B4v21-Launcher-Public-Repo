#include "gui/shiny/guiSwatchCtrl.h"

IMPLEMENT_CONOBJECT(GuiSwatchCtrl);

GuiSwatchCtrl::GuiSwatchCtrl() {
	mFilled = true;
	mBorder = true;

	mFillColor   = ColorI(255, 255, 255, 255);
	mBorderColor = ColorI(0, 0, 0, 255);
}

void GuiSwatchCtrl::initPersistFields() {
	Parent::initPersistFields();

	addGroup("Swatch");
	addField("fill", TypeBool, Offset(mFilled, GuiSwatchCtrl));
	addField("border", TypeBool, Offset(mBorder, GuiSwatchCtrl));
	addField("fillColor", TypeColorI, Offset(mFillColor, GuiSwatchCtrl));
	addField("borderColor", TypeColorI, Offset(mBorderColor, GuiSwatchCtrl));
	endGroup("Swatch");
}

void GuiSwatchCtrl::onRender(Point2I offset, const RectI &updateRect) {
	if (mFilled) dglDrawRectFill(updateRect, mFillColor);
	if (mBorder) dglDrawRect(updateRect, mBorderColor);

	renderChildControls(offset, updateRect);
}