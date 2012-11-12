/*
 * Copyright 2009, The Android Open Source Project
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RenderThemeAndroid.h"

#include "Color.h"
#include "Element.h"
#include "GraphicsContext.h"
#include "HTMLNames.h"
#include "HTMLOptionElement.h"
#include "HTMLSelectElement.h"
#include "Node.h"
#include "PlatformGraphicsContext.h"
#include "RenderSkinAndroid.h"
#include "RenderSkinButton.h"
#include "RenderSkinCombo.h"
#include "RenderSkinMediaButton.h"
#include "RenderSkinRadio.h"
#include "SkCanvas.h"
#include "UserAgentStyleSheets.h"

#if ENABLE(VIDEO)
#include "Gradient.h"
#include "HTMLMediaElement.h"
#include "MediaControlElements.h"
#include "RenderMediaControls.h"
#endif

namespace WebCore {

// Add a constant amount of padding to the textsize to get the final height
// of buttons, so that our button images are large enough to properly fit
// the text.
const int buttonPadding = 18;

// Add padding to the fontSize of ListBoxes to get their maximum sizes.
// Listboxes often have a specified size.  Since we change them into
// dropdowns, we want a much smaller height, which encompasses the text.
const int listboxPadding = 5;

// This is the color of selection in a textfield.  It was obtained by checking
// the color of selection in TextViews in the system.
const RGBA32 selectionColor = makeRGB(255, 146, 0);

static SkCanvas* getCanvasFromInfo(const RenderObject::PaintInfo& info)
{
    return info.context->platformContext()->mCanvas;
}

RenderTheme* theme()
{
    DEFINE_STATIC_LOCAL(RenderThemeAndroid, androidTheme, ());
    return &androidTheme;
}

PassRefPtr<RenderTheme> RenderTheme::themeForPage(Page* page)
{
    static RenderTheme* rt = RenderThemeAndroid::create().releaseRef();
    return rt;
}

PassRefPtr<RenderTheme> RenderThemeAndroid::create()
{
    return adoptRef(new RenderThemeAndroid());
}

RenderThemeAndroid::RenderThemeAndroid()
{
}

RenderThemeAndroid::~RenderThemeAndroid()
{
}

void RenderThemeAndroid::close()
{
}

bool RenderThemeAndroid::stateChanged(RenderObject* obj, ControlState state) const
{
    if (CheckedState == state) {
        obj->repaint();
        return true;
    }
    return false;
}

Color RenderThemeAndroid::platformActiveSelectionBackgroundColor() const
{
    return Color(selectionColor);
}

Color RenderThemeAndroid::platformInactiveSelectionBackgroundColor() const
{
    return Color(Color::transparent);
}

Color RenderThemeAndroid::platformActiveSelectionForegroundColor() const
{
    return Color::black;
}

Color RenderThemeAndroid::platformInactiveSelectionForegroundColor() const
{
    return Color::black;
}

Color RenderThemeAndroid::platformTextSearchHighlightColor() const
{
    return Color(Color::transparent);
}

Color RenderThemeAndroid::platformActiveListBoxSelectionBackgroundColor() const
{
    return Color(Color::transparent);
}

Color RenderThemeAndroid::platformInactiveListBoxSelectionBackgroundColor() const
{
    return Color(Color::transparent);
}

Color RenderThemeAndroid::platformActiveListBoxSelectionForegroundColor() const
{
    return Color(Color::transparent);
}

Color RenderThemeAndroid::platformInactiveListBoxSelectionForegroundColor() const
{
    return Color(Color::transparent);
}

int RenderThemeAndroid::baselinePosition(const RenderObject* obj) const
{
    // From the description of this function in RenderTheme.h:
    // A method to obtain the baseline position for a "leaf" control.  This will only be used if a baseline
    // position cannot be determined by examining child content. Checkboxes and radio buttons are examples of
    // controls that need to do this.
    //
    // Our checkboxes and radio buttons need to be offset to line up properly.
    return RenderTheme::baselinePosition(obj) - 2;
}

void RenderThemeAndroid::addIntrinsicMargins(RenderStyle* style) const
{
    // Cut out the intrinsic margins completely if we end up using a small font size
    if (style->fontSize() < 11)
        return;

    // Intrinsic margin value.
    const int m = 2;

    // FIXME: Using width/height alone and not also dealing with min-width/max-width is flawed.
    if (style->width().isIntrinsicOrAuto()) {
        if (style->marginLeft().quirk())
            style->setMarginLeft(Length(m, Fixed));
        if (style->marginRight().quirk())
            style->setMarginRight(Length(m, Fixed));
    }

    if (style->height().isAuto()) {
        if (style->marginTop().quirk())
            style->setMarginTop(Length(m, Fixed));
        if (style->marginBottom().quirk())
            style->setMarginBottom(Length(m, Fixed));
    }
}

bool RenderThemeAndroid::supportsFocus(ControlPart appearance)
{
    switch (appearance) {
    case PushButtonPart:
    case ButtonPart:
    case TextFieldPart:
        return true;
    default:
        return false;
    }

    return false;
}

void RenderThemeAndroid::adjustButtonStyle(CSSStyleSelector*, RenderStyle* style, WebCore::Element*) const
{
    // Padding code is taken from RenderThemeSafari.cpp
    // It makes sure we have enough space for the button text.
    const int padding = 8;
    style->setPaddingLeft(Length(padding, Fixed));
    style->setPaddingRight(Length(padding, Fixed));
    style->setMinHeight(Length(style->fontSize() + buttonPadding, Fixed));
}

bool RenderThemeAndroid::paintCheckbox(RenderObject* obj, const RenderObject::PaintInfo& info, const IntRect& rect)
{
    RenderSkinRadio::Draw(getCanvasFromInfo(info), obj->node(), rect, true);
    return false;
}

bool RenderThemeAndroid::paintButton(RenderObject* obj, const RenderObject::PaintInfo& info, const IntRect& rect)
{
    // If it is a disabled button, simply paint it to the master picture.
    Node* node = obj->node();
    Element* formControlElement = static_cast<Element*>(node);
    if (formControlElement && !formControlElement->isEnabledFormControl())
        RenderSkinButton::Draw(getCanvasFromInfo(info), rect, RenderSkinAndroid::kDisabled);
    else
        // Store all the important information in the platform context.
        info.context->platformContext()->storeButtonInfo(node, rect);

    // We always return false so we do not request to be redrawn.
    return false;
}

#if ENABLE(VIDEO)

// Helper class to transform the context's matrix to the object's content area,
// scaled to 0,0,100,100. Used to draw the vector media control buttons.
class VectorMediaControlTransformer : public Noncopyable {
public:
    VectorMediaControlTransformer(GraphicsContext* context, RenderObject* obj, const IntRect& r) : m_context(context)
    {
        RenderStyle* style = obj->style();
        m_context->save();
        m_context->translate(FloatSize(r.x() + style->paddingLeft().value(), r.y() + style->paddingTop().value()));
        m_context->scale(FloatSize((r.width() - style->paddingLeft().value() - style->paddingRight().value()) / 100.0,
             (r.height() - style->paddingTop().value() - style->paddingBottom().value()) / 100.0));
    }

    ~VectorMediaControlTransformer()
    {
        m_context->restore();
    }

private:
    GraphicsContext* m_context;
};

static bool hasSource(const HTMLMediaElement* mediaElement)
{
    return mediaElement->networkState() != HTMLMediaElement::NETWORK_EMPTY
        && mediaElement->networkState() != HTMLMediaElement::NETWORK_NO_SOURCE;
}

String RenderThemeAndroid::extraMediaControlsStyleSheet()
{
    return String(mediaControlsAndroidUserAgentStyleSheet, sizeof(mediaControlsAndroidUserAgentStyleSheet));
}

bool RenderThemeAndroid::shouldRenderMediaControlPart(ControlPart part, Element* e)
{
    HTMLMediaElement* mediaElement = static_cast<HTMLMediaElement*>(e);

    switch (part) {
    case MediaPlayButtonPart:
    case MediaSliderPart:
    case MediaSliderThumbPart:
    case MediaControlsBackgroundPart:
    case MediaCurrentTimePart:
    case MediaTimeRemainingPart:
        return true;
    case MediaFullscreenButtonPart:
        return mediaElement->supportsFullscreen();
    case MediaMuteButtonPart:
        return !mediaElement->supportsFullscreen();
    // Android devices should control the volume through the dedicated hardware keys.
    case MediaVolumeSliderThumbPart:
    case MediaVolumeSliderContainerPart:
    case MediaVolumeSliderPart:
    default:
        return false;
    }
}

void RenderThemeAndroid::adjustSliderThumbSize(RenderObject* o) const
{
    ControlPart part = o->style()->appearance();
    RenderStyle* parentStyle = o->parent()->style();

    // FIXME: Other ports get the dimensions from an image. It would be
    // nice if there was a CSS rule for the slider thumb.

    if (part == MediaSliderThumbPart) {
        const int mediaSliderWidth = 9;
        const int mediaSliderHeight = 19;
        o->style()->setWidth(Length(mediaSliderWidth, Fixed));
        o->style()->setHeight(Length(mediaSliderHeight, Fixed));
    } else if (part == MediaVolumeSliderThumbPart) {
        const int mediaSliderWidth = 14;
        const int mediaSliderHeight = 7;
        o->style()->setWidth(Length(mediaSliderWidth, Fixed));
        o->style()->setHeight(Length(mediaSliderHeight, Fixed));
    }
}

bool RenderThemeAndroid::paintMediaControlsBackground(RenderObject* obj, const RenderObject::PaintInfo& info, const IntRect& rect)
{
    if (rect.isEmpty())
        return true;

    HTMLMediaElement* mediaElement = toParentMediaElement(obj);
    if (!mediaElement)
        return false;

    GraphicsContext* context = info.context;
    Color originalColor = context->strokeColor();
    float originalThickness = context->strokeThickness();
    StrokeStyle originalStyle = context->strokeStyle();

    context->setStrokeStyle(SolidStroke);

    // Draw the left border using CSS defined width and color.
    context->setStrokeThickness(obj->style()->borderLeftWidth());
    context->setStrokeColor(obj->style()->borderLeftColor().rgb(), DeviceColorSpace);
    context->drawLine(IntPoint(rect.x() + 1, rect.y()),
                        IntPoint(rect.x() + 1, rect.y() + rect.height()));

    // Draw the right border using CSS defined width and color.
    context->setStrokeThickness(obj->style()->borderRightWidth());
    context->setStrokeColor(obj->style()->borderRightColor().rgb(), DeviceColorSpace);
    context->drawLine(IntPoint(rect.x() + rect.width() - 1, rect.y()),
                        IntPoint(rect.x() + rect.width() - 1, rect.y() + rect.height()));

    context->setStrokeColor(originalColor, DeviceColorSpace);
    context->setStrokeThickness(originalThickness);
    context->setStrokeStyle(originalStyle);

    return true;
}

bool RenderThemeAndroid::paintMediaFullscreenButton(RenderObject* obj, const RenderObject::PaintInfo& info, const IntRect& rect)
{
    HTMLMediaElement* mediaElement = toParentMediaElement(obj);
    if (!mediaElement)
        return false;

    GraphicsContext* context = info.context;
    VectorMediaControlTransformer transformer(context, obj, rect);

    context->setStrokeStyle(SolidStroke);
    context->setStrokeThickness(10);
    context->setStrokeColor(Color::white, DeviceColorSpace);
    context->setFillColor(Color::white, DeviceColorSpace);
    context->setShouldAntialias(true);

    context->beginPath();

    // Draw the fullscreen button as two diagonal lines crossing with arrows at
    // the ends.

    // Top left arrow head.
    Path path;
    path.moveTo(FloatPoint(0, 0));
    path.addLineTo(FloatPoint(30, 0));
    path.addLineTo(FloatPoint(30, 10));
    path.addLineTo(FloatPoint(10, 10));
    path.addLineTo(FloatPoint(10, 30));
    path.addLineTo(FloatPoint(0, 30));
    path.closeSubpath();

    context->addPath(path);

    // Top right arrow head.
    path.clear();
    path.moveTo(FloatPoint(70, 0));
    path.addLineTo(FloatPoint(100, 0));
    path.addLineTo(FloatPoint(100, 30));
    path.addLineTo(FloatPoint(90, 30));
    path.addLineTo(FloatPoint(90, 10));
    path.addLineTo(FloatPoint(70, 10));
    path.closeSubpath();

    context->addPath(path);

    // Bottom left arrow head.
    path.clear();
    path.moveTo(FloatPoint(0, 70));
    path.addLineTo(FloatPoint(0, 100));
    path.addLineTo(FloatPoint(30, 100));
    path.addLineTo(FloatPoint(30, 90));
    path.addLineTo(FloatPoint(10, 90));
    path.addLineTo(FloatPoint(10, 70));
    path.closeSubpath();

    context->addPath(path);

    // Bottom right arrow head.
    path.clear();
    path.moveTo(FloatPoint(70, 100));
    path.addLineTo(FloatPoint(100, 100));
    path.addLineTo(FloatPoint(100, 70));
    path.addLineTo(FloatPoint(90, 70));
    path.addLineTo(FloatPoint(90, 90));
    path.addLineTo(FloatPoint(70, 90));
    path.closeSubpath();

    context->addPath(path);
    context->fillPath();

    context->drawLine(IntPoint(10, 10), IntPoint(90, 90));
    context->drawLine(IntPoint(10, 90), IntPoint(90, 10));

    return true;
}

bool RenderThemeAndroid::paintMediaMuteButton(RenderObject* obj, const RenderObject::PaintInfo& info, const IntRect& rect)
{
    // This button will only be displayed if the fullscreen isn't supported,
    // otherwise the fullscreen button will be in its place.
    HTMLMediaElement* mediaElement = toParentMediaElement(obj);
    if (!mediaElement)
        return false;

    GraphicsContext* context = info.context;
    VectorMediaControlTransformer transformer(context, obj, rect);

    // Draw the mute polygon.
    Path path;
    path.moveTo(FloatPoint(20, 30));
    path.addLineTo(FloatPoint(50, 30));
    path.addLineTo(FloatPoint(80, 0));
    path.addLineTo(FloatPoint(80, 100));
    path.addLineTo(FloatPoint(50, 70));
    path.addLineTo(FloatPoint(20, 70));
    path.closeSubpath();

    context->beginPath();
    context->addPath(path);

    context->setShouldAntialias(true);
    context->setFillColor(Color::white, DeviceColorSpace);

    context->fillPath();

    if (mediaElement->muted()) {
        // Draws a red line across the mute polygon to indicate state.
        context->setStrokeColor(0xFFFF0000, DeviceColorSpace);
        context->setStrokeStyle(SolidStroke);
        context->setStrokeThickness(10);
        context->drawLine(IntPoint(0, 100), IntPoint(100, 0));
    }

    return true;
}

bool RenderThemeAndroid::paintMediaPlayButton(RenderObject* obj, const RenderObject::PaintInfo& info, const IntRect& rect)
{
    HTMLMediaElement* mediaElement = toParentMediaElement(obj);
    if (!mediaElement)
        return false;

    GraphicsContext* context = info.context;
    VectorMediaControlTransformer transformer(context, obj, rect);

    context->setShouldAntialias(true);
    context->setFillColor(Color::white, DeviceColorSpace);

    if (mediaElement->canPlay()) {
        // Draw the play triangle.
        Path path;
        path.moveTo(FloatPoint(0, 0));
        path.addLineTo(FloatPoint(100, 50));
        path.addLineTo(FloatPoint(0, 100));
        path.closeSubpath();

        context->beginPath();
        context->addPath(path);
        context->fillPath();
    } else {
        // We are paused. Draw the pause button.
        context->fillRect(FloatRect(0, 0, 30, 100));
        context->fillRect(FloatRect(70, 0, 30, 100));
    }

    return false;
}

bool RenderThemeAndroid::paintMediaSliderThumb(RenderObject* obj, const RenderObject::PaintInfo& info, const IntRect& rect)
{
    if (!obj->parent()->isSlider())
        return false;

    HTMLMediaElement* mediaElement = toParentMediaElement(obj->parent());
    if (!mediaElement)
        return false;

    if (!hasSource(mediaElement))
        return true;

    GraphicsContext* context = info.context;
    context->save();

    context->setShouldAntialias(true);
    context->setFillColor(Color::white, DeviceColorSpace);

    context->fillRoundedRect(rect, IntSize(3, 3), IntSize(3, 3), IntSize(3, 3), IntSize(3, 3), Color::white, DeviceColorSpace);
    context->restore();

    return true;
}

bool RenderThemeAndroid::paintMediaSliderTrack(RenderObject* obj, const RenderObject::PaintInfo& info, const IntRect& rect)
{
    HTMLMediaElement* mediaElement = toParentMediaElement(obj);
    if (!mediaElement)
        return false;

    RenderStyle* style = obj->style();
    GraphicsContext* context = info.context;

    context->save();
    context->setShouldAntialias(true);
    context->setStrokeStyle(SolidStroke);
    context->setStrokeColor(style->borderLeftColor(), DeviceColorSpace);
    context->setStrokeThickness(style->borderLeftWidth());
    context->setFillColor(style->backgroundColor(), DeviceColorSpace);
    context->drawRect(rect);
    context->restore();

    IntRect bufferedRect = rect;
    bufferedRect.inflate(-style->borderLeftWidth());
    bufferedRect.setWidth((bufferedRect.width() * mediaElement->percentLoaded()));

    // Don't bother drawing an empty area.
    if (!bufferedRect.isEmpty()) {
        IntPoint sliderTopLeft = bufferedRect.location();
        IntPoint sliderTopRight = sliderTopLeft;
        sliderTopRight.move(0, bufferedRect.height());

        RefPtr<Gradient> gradient = Gradient::create(sliderTopLeft, sliderTopRight);
        Color startColor = style->color();
        gradient->addColorStop(0.0, startColor);
        gradient->addColorStop(1.0, Color(startColor.red() / 2, startColor.green() / 2, startColor.blue() / 2, startColor.alpha()));

        context->save();
        context->setStrokeStyle(NoStroke);
        context->setFillGradient(gradient);
        context->fillRect(bufferedRect);
        context->restore();
    }

    return true;
}

bool RenderThemeAndroid::paintMediaVolumeSliderThumb(RenderObject* obj, const RenderObject::PaintInfo& info, const IntRect& rect)
{
    // We are not painting the volume slider as a UI decision. This code won't
    // be called unless the volume media control parts are enabled in
    // shouldRenderMediaControlPart.
    if (!obj->parent()->isSlider())
        return false;

    HTMLMediaElement* mediaElement = toParentMediaElement(obj->parent());
    if (!mediaElement)
        return false;

    if (!hasSource(mediaElement))
        return true;

    GraphicsContext* context = info.context;

    context->save();
    context->setStrokeStyle(SolidStroke);
    context->setStrokeColor(Color::white, DeviceColorSpace);
    context->setFillColor(Color::white, DeviceColorSpace);
    context->drawRect(rect);
    context->restore();

    return true;
}

bool RenderThemeAndroid::paintMediaVolumeSliderTrack(RenderObject* obj, const RenderObject::PaintInfo& info, const IntRect& rect)
{
    // We are not painting the volume slider as a UI decision. This code won't
    // be called unless the volume media control parts are enabled in
    // shouldRenderMediaControlPart.
    HTMLMediaElement* mediaElement = toParentMediaElement(obj);
    if (!mediaElement)
        return false;

    GraphicsContext* context = info.context;
    Color originalColor = context->strokeColor();
    if (originalColor != Color::white)
        context->setStrokeColor(Color::white, DeviceColorSpace);

    int x = rect.x() + rect.width() / 2;
    context->drawLine(IntPoint(x, rect.y()),  IntPoint(x, rect.y() + rect.height()));

    if (originalColor != Color::white)
        context->setStrokeColor(originalColor, DeviceColorSpace);
    return true;
}

#endif

bool RenderThemeAndroid::paintRadio(RenderObject* obj, const RenderObject::PaintInfo& info, const IntRect& rect)
{
    RenderSkinRadio::Draw(getCanvasFromInfo(info), obj->node(), rect, false);
    return false;
}

void RenderThemeAndroid::setCheckboxSize(RenderStyle* style) const
{
    style->setWidth(Length(19, Fixed));
    style->setHeight(Length(19, Fixed));
}

void RenderThemeAndroid::setRadioSize(RenderStyle* style) const
{
    // This is the same as checkboxes.
    setCheckboxSize(style);
}

void RenderThemeAndroid::adjustTextFieldStyle(CSSStyleSelector*, RenderStyle* style, WebCore::Element*) const
{
    addIntrinsicMargins(style);
}

bool RenderThemeAndroid::paintTextField(RenderObject*, const RenderObject::PaintInfo&, const IntRect&)
{
    return true;
}

void RenderThemeAndroid::adjustTextAreaStyle(CSSStyleSelector*, RenderStyle* style, WebCore::Element*) const
{
    addIntrinsicMargins(style);
}

bool RenderThemeAndroid::paintTextArea(RenderObject* obj, const RenderObject::PaintInfo& info, const IntRect& rect)
{
    if (!obj->isListBox())
        return true;

    paintCombo(obj, info, rect);
    RenderStyle* style = obj->style();
    if (style)
        style->setColor(Color::transparent);
    Node* node = obj->node();
    if (!node || !node->hasTagName(HTMLNames::selectTag))
        return true;

    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(node);
    // The first item may be visible.  Make sure it does not draw.
    // If it has a style, it overrides the RenderListBox's style, so we
    // need to make sure both are set to transparent.
    node = select->item(0);
    if (node) {
        RenderObject* renderer = node->renderer();
        if (renderer) {
            RenderStyle* renderStyle = renderer->style();
            if (renderStyle)
                renderStyle->setColor(Color::transparent);
        }
    }
    // Find the first selected option, and draw its text.
    // FIXME: In a later change, if there is more than one item selected,
    // draw a string that says "X items" like iPhone Safari does
    int index = select->selectedIndex();
    node = select->item(index);
    if (!node || !node->hasTagName(HTMLNames::optionTag))
        return true;

    HTMLOptionElement* option = static_cast<HTMLOptionElement*>(node);
    String label = option->textIndentedToRespectGroupLabel();
    SkRect r(rect);

    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setTextEncoding(SkPaint::kUTF16_TextEncoding);
    // Values for text size and positioning determined by trial and error
    paint.setTextSize(r.height() - SkIntToScalar(6));

    SkCanvas* canvas = getCanvasFromInfo(info);
    int saveCount = canvas->save();
    r.fRight -= SkIntToScalar(RenderSkinCombo::extraWidth());
    canvas->clipRect(r);
    canvas->drawText(label.characters(), label.length() << 1,
             r.fLeft + SkIntToScalar(5), r.fBottom - SkIntToScalar(5), paint);
    canvas->restoreToCount(saveCount);

    return true;
}

void RenderThemeAndroid::adjustSearchFieldStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    addIntrinsicMargins(style);
}

bool RenderThemeAndroid::paintSearchField(RenderObject*, const RenderObject::PaintInfo&, const IntRect&)
{
    return true;
}

void RenderThemeAndroid::adjustListboxStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    style->setPaddingRight(Length(RenderSkinCombo::extraWidth(), Fixed));
    style->setMaxHeight(Length(style->fontSize() + listboxPadding, Fixed));
    // Make webkit draw invisible, since it will simply draw the first element
    style->setColor(Color::transparent);
    addIntrinsicMargins(style);
}

static void adjustMenuListStyleCommon(RenderStyle* style, Element* e)
{
    // Added to make room for our arrow and make the touch target less cramped.
    style->setPaddingLeft(Length(RenderSkinCombo::padding(), Fixed));
    style->setPaddingTop(Length(RenderSkinCombo::padding(), Fixed));
    style->setPaddingBottom(Length(RenderSkinCombo::padding(), Fixed));
    style->setPaddingRight(Length(RenderSkinCombo::extraWidth(), Fixed));
}

void RenderThemeAndroid::adjustMenuListStyle(CSSStyleSelector*, RenderStyle* style, Element* e) const
{
    adjustMenuListStyleCommon(style, e);
    addIntrinsicMargins(style);
}

bool RenderThemeAndroid::paintCombo(RenderObject* obj, const RenderObject::PaintInfo& info,  const IntRect& rect)
{
    if (obj->style() && !obj->style()->backgroundColor().alpha())
        return true;
    return RenderSkinCombo::Draw(getCanvasFromInfo(info), obj->node(), rect.x(), rect.y(), rect.width(), rect.height());
}

bool RenderThemeAndroid::paintMenuList(RenderObject* obj, const RenderObject::PaintInfo& info, const IntRect& rect)
{
    return paintCombo(obj, info, rect);
}

void RenderThemeAndroid::adjustMenuListButtonStyle(CSSStyleSelector*, RenderStyle* style, Element* e) const
{
    // Copied from RenderThemeSafari.
    const float baseFontSize = 11.0f;
    const int baseBorderRadius = 5;
    float fontScale = style->fontSize() / baseFontSize;

    style->resetPadding();
    style->setBorderRadius(IntSize(int(baseBorderRadius + fontScale - 1), int(baseBorderRadius + fontScale - 1))); // FIXME: Round up?

    const int minHeight = 15;
    style->setMinHeight(Length(minHeight, Fixed));

    style->setLineHeight(RenderStyle::initialLineHeight());
    // Found these padding numbers by trial and error.
    const int padding = 4;
    style->setPaddingTop(Length(padding, Fixed));
    style->setPaddingLeft(Length(padding, Fixed));
    adjustMenuListStyleCommon(style, e);
}

bool RenderThemeAndroid::paintMenuListButton(RenderObject* obj, const RenderObject::PaintInfo& info, const IntRect& rect)
{
    return paintCombo(obj, info, rect);
}

bool RenderThemeAndroid::supportsFocusRing(const RenderStyle* style) const
{
    return style->opacity() > 0
        && style->hasAppearance()
        && style->appearance() != TextFieldPart
        && style->appearance() != SearchFieldPart
        && style->appearance() != TextAreaPart
        && style->appearance() != CheckboxPart
        && style->appearance() != RadioPart
        && style->appearance() != PushButtonPart
        && style->appearance() != SquareButtonPart
        && style->appearance() != ButtonPart
        && style->appearance() != ButtonBevelPart
        && style->appearance() != MenulistPart
        && style->appearance() != MenulistButtonPart;
}

} // namespace WebCore
