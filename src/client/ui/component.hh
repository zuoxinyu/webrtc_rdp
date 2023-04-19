#include <string>
extern "C" {
#include "microui.h"
}
namespace mui
{

using Context = mu_Context *;
using Id = mu_Id;
using Real = mu_Real;
using Font = mu_Font;
using Style = mu_Style;
using Option = int;

using Vec = mu_Vec2;
using Rect = mu_Rect;
using Color = mu_Color;
using Icon = int;

using Layout = mu_Layout;
using Container = mu_Container;

class Component
{
  protected:
    Context ctx_;

  public:
    Component(Context ctx) : ctx_(ctx){};
    virtual ~Component() = 0;

    virtual void render() = 0;
};

class Window : public Component
{

    std::string title_;
    Rect rect_;
    Option opt_;

  public:
    Window(Context ctx) : Component(ctx) {}
    ~Window() override = 0;

    virtual void render_content() = 0;
    void render() override
    {
        if (mu_begin_window_ex(ctx_, title_.c_str(), rect_, opt_)) {
            render_content();
            mu_end_window(ctx_);
        }
    }
};

class Popup : public Component
{
    std::string name_;

  public:
    Popup(Context ctx) : Component(ctx) {}
    ~Popup() override = 0;
};

class Label : public Component
{
};

class Button : public Component
{
    std::string label_;
    Icon icon_;

  public:
};

class Checkbox : public Component
{
};
} // namespace mui
