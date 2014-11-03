/*
 * AnimatedPopup.cxx
 *
 *  Created on: Jun 18, 2013
 *      Author: tom
 */

#include "common/PreviewWindow.hpp"
#include "slotdata/text_popup.hpp"

namespace LinksRouting
{
  PreviewWindow::~PreviewWindow() {}

namespace SlotType
{

  //----------------------------------------------------------------------------
  const double AnimatedPopup::SHOW_DELAY = 0.2;
  const double AnimatedPopup::HIDE_DELAY = 0.4;
  const double AnimatedPopup::TIMEOUT = 0.6;
  bool AnimatedPopup::debug = false;

  //----------------------------------------------------------------------------
  AnimatedPopup::AnimatedPopup(bool visible):
    _state(visible ? VISIBLE : HIDDEN),
    _time(0)
  {

  }

  //----------------------------------------------------------------------------
  bool AnimatedPopup::update(double dt)
  {
    if( !(_state & TRANSIENT) )
      return false;

    if( (_time -= dt) <= 0 )
    {
      if( _state & DELAYED )
      {
        _state &= ~DELAYED;
        _time = TIMEOUT;
      }
      else
        _state &= ~TRANSIENT;
    }

    if( debug )
      std::cout << this
                << " transient, time = " << _time << ", dt = " << dt << ", "
                << (_state & DELAYED ? "delayed " : "")
                << (_state & VISIBLE ? "show" : "hide")
                << ", alpha = " << getAlpha()
                << std::endl;

    return true;
  }

  //----------------------------------------------------------------------------
  void AnimatedPopup::show()
  {
    setFlags(VISIBLE);
  }

  //----------------------------------------------------------------------------
  void AnimatedPopup::hide()
  {
    setFlags(HIDDEN);
  }

  //----------------------------------------------------------------------------
  void AnimatedPopup::fadeIn()
  {
    setFlags(TRANSIENT | VISIBLE);
  }

  //----------------------------------------------------------------------------
  void AnimatedPopup::fadeOut()
  {
    setFlags(TRANSIENT | HIDDEN);
  }

  //----------------------------------------------------------------------------
  void AnimatedPopup::delayedFadeIn()
  {
    setFlags(DELAYED | TRANSIENT | VISIBLE);
  }

  //----------------------------------------------------------------------------
  void AnimatedPopup::delayedFadeOut()
  {
    setFlags(DELAYED | TRANSIENT | HIDDEN);
  }

  template<int N, typename T>
  static T pow(T base)
  {
    return (N < 0)
      ? (1. / pow<-N>(base))
      : (  ((N & 1) ? base : 1)
        * ((N > 1) ? pow<N / 2>(base * base) : 1)
        );
  }

  //----------------------------------------------------------------------------
  double AnimatedPopup::getAlpha() const
  {
    bool visible = _state & VISIBLE;
    if( !(_state & TRANSIENT) )
      return visible;
    else if( _state & DELAYED )
      return !visible;
    else
      return visible ? 1 - pow<4>(_time / TIMEOUT)
                     : pow<4>(_time / TIMEOUT);
  }

  //----------------------------------------------------------------------------
  bool AnimatedPopup::isVisible() const
  {
    return getAlpha() > 0.05;
  }

  //----------------------------------------------------------------------------
  bool AnimatedPopup::isTransient() const
  {
    return _state & TRANSIENT;
  }

  //----------------------------------------------------------------------------
  bool AnimatedPopup::isFadeIn() const
  {
    return (_state & TRANSIENT) && (_state & VISIBLE);
  }

  //----------------------------------------------------------------------------
  bool AnimatedPopup::isFadeOut() const
  {
    return (_state & TRANSIENT) && !(_state & VISIBLE);
  }

  //----------------------------------------------------------------------------
  void AnimatedPopup::setFlags(uint16_t flags)
  {
    if( flags & DELAYED )
      _time = (flags & VISIBLE) ? SHOW_DELAY : HIDE_DELAY;
    else if( flags & TRANSIENT )
      _time = (_state & VISIBLE) == (flags & VISIBLE) ? _time : TIMEOUT - _time;

    _state = flags;
  }

  //----------------------------------------------------------------------------
  TextPopup::Popup::~Popup()
  {
    if( preview )
      preview->release();
    preview = nullptr;
  }

} // namespace SlotType
} // namespace LinksRouting
