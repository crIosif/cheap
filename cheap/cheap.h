#pragma once

#include <exception>
#include <format> // TODO: configurable to use fmt
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>


namespace cheap
{

   struct cheap_exception final : std::runtime_error
   {
      using runtime_error::runtime_error;
   };

   struct bool_attribute {
      bool m_value = true; // TODO: setting this to false
      std::string m_name;
   };
   struct string_attribute {
      std::string m_name;
      std::string m_data;
   };
   using attribute = std::variant<bool_attribute, string_attribute>;

   namespace detail
   {
      constexpr auto get_attribute_name(const attribute& attrib) -> std::string
      {
         return std::visit(
            [](const auto& alternative) {return alternative.m_name; },
            attrib
         );
      }


      constexpr auto is_in(std::span<const std::string_view> choices, const std::string& value) -> bool
      {
         for (const auto& test : choices)
         {
            if (test == value)
               return true;
         }
         return false;
      }


      template<typename target_type>
      auto assert_attribute_type(const attribute& attrib) -> void
      {
         if (std::holds_alternative<target_type>(attrib) == false)
         {
            const std::string target_type_str = std::same_as<target_type, bool_attribute> ? "bool attribute" : "string attribute";
            throw cheap_exception{ std::format("Attribute {} must be a {}. It's not!", get_attribute_name(attrib), target_type_str) };
         }
      }


      auto assert_enum_choice(const string_attribute& attrib, std::span<const std::string_view> choices) -> void
      {
         if (is_in(choices, attrib.m_data) == false)
         {
            std::string choices_str;
            for (int i = 0; i < std::size(choices); ++i)
            {
               if (i > 0)
                  choices_str += ", ";
               choices_str += choices[i];
            }
            throw cheap_exception{ std::format("Attribute is an enum. It must be one of [{}]. But it's {}!", choices_str, attrib.m_data) };
         }
      }


      constexpr auto assert_attrib_valid(const attribute& attrib) -> void
      {
         // Check if global boolean attributes are used correctly
         constexpr std::string_view bool_list[] = { "autofocus", "hidden", "itemscope" };
         const auto attrib_name = get_attribute_name(attrib);
         if (is_in(bool_list, attrib_name) && std::holds_alternative<bool_attribute>(attrib) == false)
         {
            throw cheap_exception{ std::format("{} attribute must be bool. It is not!", attrib_name) };
         }

         // TODO: string list (id etc)

         // Check if global enumerated attributes are used correctly
         if (attrib_name == "autocapitalize")
         {
            assert_attribute_type<string_attribute>(attrib);
            constexpr std::string_view valid_list[] = { "off", "on", "sentences", "words", "characters" };
            assert_enum_choice(std::get<string_attribute>(attrib), valid_list);
         }
         else if (attrib_name == "contenteditable")
         {
            assert_attribute_type<string_attribute>(attrib);
            constexpr std::string_view valid_list[] = { "true", "false" };
            assert_enum_choice(std::get<string_attribute>(attrib), valid_list);
         }
         else if (attrib_name == "dir")
         {
            assert_attribute_type<string_attribute>(attrib);
            constexpr std::string_view valid_list[] = { "ltr", "rtl", "auto" };
            assert_enum_choice(std::get<string_attribute>(attrib), valid_list);
         }
         else if (attrib_name == "draggable")
         {
            assert_attribute_type<string_attribute>(attrib);
            constexpr std::string_view valid_list[] = { "true", "false" };
            assert_enum_choice(std::get<string_attribute>(attrib), valid_list);
         }
         else if (attrib_name == "enterkeyhint")
         {
            assert_attribute_type<string_attribute>(attrib);
            constexpr std::string_view valid_list[] = { "enter", "done", "go", "next", "previous", "search", "send" };
            assert_enum_choice(std::get<string_attribute>(attrib), valid_list);
         }
         else if (attrib_name == "inputmode")
         {
            assert_attribute_type<string_attribute>(attrib);
            constexpr std::string_view valid_list[] = { "none", "text", "decimal", "numeric", "tel", "search", "email", "url" };
            assert_enum_choice(std::get<string_attribute>(attrib), valid_list);
         }
      }
   } // namespace detail


   auto operator "" _att(const char* c_str, std::size_t size) -> attribute
   {
      std::string str(c_str);
      const auto equal_pos = str.find('=');
      attribute result;
      if (equal_pos == std::string::npos)
      {
         result = bool_attribute{ .m_name = str };
      }
      else
      {
         result = string_attribute{
            .m_name = str.substr(0, equal_pos),
            .m_data = str.substr(equal_pos + 1)
         };
      }
      detail::assert_attrib_valid(result);
      return result;
   }

   struct element;
   using content = std::variant<element, std::string>;


   struct element
   {
      std::string m_type;
      std::vector<attribute> m_attributes;
      std::vector<content> m_inner_html;

      explicit element() = default; // TODO: maybe debug tag
      explicit element(const std::string_view name, const std::vector<attribute>& attributes, const std::vector<content>& inner_html)
         : m_type(name)
         , m_attributes(attributes)
         , m_inner_html(inner_html)
      { }
      explicit element(const std::string_view name, const std::vector<content>& inner_html)
         : element(name, {}, inner_html)
      { }

      [[nodiscard]] auto get_trivial() const -> std::optional<std::string>
      {
         if (m_inner_html.empty())
            return "";
         if (m_inner_html.size() != 1 || std::holds_alternative<std::string>(m_inner_html.front()) == false)
            return std::nullopt;
         return std::get<std::string>(m_inner_html.front());
      }
   };


   namespace detail
   {
      template<typename T, typename ... types>
      constexpr bool is_any_of = (std::same_as<T, types> || ...);

      template<typename T>
      auto process(element& result, T&& arg) -> void
      {
         if constexpr (is_any_of<T, element, attribute, std::string> == false)
         {
            process(result, std::string{ arg });
         }
         else if constexpr (std::same_as<T, std::string>)
         {
            // string as first parameter -> element name
            if (result.m_type.empty())
            {
               result.m_type = arg;
            }

            // otherwise -> content
            else
            {
               if (result.m_inner_html.empty() == false)
               {
                  // TODO: error
               }
               result.m_inner_html = { arg };
            }
         }
         else if constexpr (std::same_as<T, attribute>)
         {
            result.m_attributes.push_back(arg);
         }
         else if constexpr (std::same_as<T, element>)
         {
            result.m_inner_html.push_back(arg);
         }
      }
   } // namespace detail

   template<typename ... Ts>
   auto create_element(Ts&&... args) -> element
   {
      // TODO: assert that a name is set
      element result{};
      (detail::process(result, std::forward<Ts>(args)), ...);
      return result;
   }

   template<typename ... Ts> auto html(Ts&&... args) -> element { return create_element("html", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto base(Ts&&... args) -> element { return create_element("base", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto head(Ts&&... args) -> element { return create_element("head", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto link(Ts&&... args) -> element { return create_element("link", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto meta(Ts&&... args) -> element { return create_element("meta", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto style(Ts&&... args) -> element { return create_element("style", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto title(Ts&&... args) -> element { return create_element("title", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto body(Ts&&... args) -> element { return create_element("body", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto address(Ts&&... args) -> element { return create_element("address", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto article(Ts&&... args) -> element { return create_element("article", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto aside(Ts&&... args) -> element { return create_element("aside", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto footer(Ts&&... args) -> element { return create_element("footer", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto header(Ts&&... args) -> element { return create_element("header", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto h1(Ts&&... args) -> element { return create_element("h1", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto h2(Ts&&... args) -> element { return create_element("h2", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto h3(Ts&&... args) -> element { return create_element("h3", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto h4(Ts&&... args) -> element { return create_element("h4", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto h5(Ts&&... args) -> element { return create_element("h5", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto h6(Ts&&... args) -> element { return create_element("h6", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto main(Ts&&... args) -> element { return create_element("main", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto nav(Ts&&... args) -> element { return create_element("nav", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto section(Ts&&... args) -> element { return create_element("section", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto blockquote(Ts&&... args) -> element { return create_element("blockquote", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto dd(Ts&&... args) -> element { return create_element("dd", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto div(Ts&&... args) -> element { return create_element("div", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto dl(Ts&&... args) -> element { return create_element("dl", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto dt(Ts&&... args) -> element { return create_element("dt", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto figcaption(Ts&&... args) -> element { return create_element("figcaption", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto figure(Ts&&... args) -> element { return create_element("figure", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto hr(Ts&&... args) -> element { return create_element("hr", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto menu(Ts&&... args) -> element { return create_element("menu", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto ol(Ts&&... args) -> element { return create_element("ol", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto p(Ts&&... args) -> element { return create_element("p", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto pre(Ts&&... args) -> element { return create_element("pre", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto ul(Ts&&... args) -> element { return create_element("ul", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto a(Ts&&... args) -> element { return create_element("a", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto abbr(Ts&&... args) -> element { return create_element("abbr", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto b(Ts&&... args) -> element { return create_element("b", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto bdi(Ts&&... args) -> element { return create_element("bdi", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto bdo(Ts&&... args) -> element { return create_element("bdo", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto cite(Ts&&... args) -> element { return create_element("cite", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto code(Ts&&... args) -> element { return create_element("code", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto data(Ts&&... args) -> element { return create_element("data", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto dfn(Ts&&... args) -> element { return create_element("dfn", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto em(Ts&&... args) -> element { return create_element("em", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto i(Ts&&... args) -> element { return create_element("i", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto kdb(Ts&&... args) -> element { return create_element("kdb", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto mark(Ts&&... args) -> element { return create_element("mark", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto q(Ts&&... args) -> element { return create_element("q", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto rp(Ts&&... args) -> element { return create_element("rp", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto rt(Ts&&... args) -> element { return create_element("rt", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto ruby(Ts&&... args) -> element { return create_element("ruby", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto s(Ts&&... args) -> element { return create_element("s", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto samp(Ts&&... args) -> element { return create_element("samp", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto small(Ts&&... args) -> element { return create_element("small", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto span(Ts&&... args) -> element { return create_element("span", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto strong(Ts&&... args) -> element { return create_element("strong", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto sub(Ts&&... args) -> element { return create_element("sub", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto sup(Ts&&... args) -> element { return create_element("sup", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto time(Ts&&... args) -> element { return create_element("time", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto u(Ts&&... args) -> element { return create_element("u", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto var(Ts&&... args) -> element { return create_element("var", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto wbr(Ts&&... args) -> element { return create_element("wbr", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto area(Ts&&... args) -> element { return create_element("area", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto audio(Ts&&... args) -> element { return create_element("audio", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto img(Ts&&... args) -> element { return create_element("img", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto map(Ts&&... args) -> element { return create_element("map", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto track(Ts&&... args) -> element { return create_element("track", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto video(Ts&&... args) -> element { return create_element("video", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto embed(Ts&&... args) -> element { return create_element("embed", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto iframe(Ts&&... args) -> element { return create_element("iframe", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto object(Ts&&... args) -> element { return create_element("object", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto picture(Ts&&... args) -> element { return create_element("picture", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto portal(Ts&&... args) -> element { return create_element("portal", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto source(Ts&&... args) -> element { return create_element("source", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto svg(Ts&&... args) -> element { return create_element("svg", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto math(Ts&&... args) -> element { return create_element("math", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto canvas(Ts&&... args) -> element { return create_element("canvas", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto noscript(Ts&&... args) -> element { return create_element("noscript", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto script(Ts&&... args) -> element { return create_element("script", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto del(Ts&&... args) -> element { return create_element("del", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto ins(Ts&&... args) -> element { return create_element("ins", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto caption(Ts&&... args) -> element { return create_element("caption", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto col(Ts&&... args) -> element { return create_element("col", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto colgroup(Ts&&... args) -> element { return create_element("colgroup", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto stable(Ts&&... args) -> element { return create_element("stable", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto tbody(Ts&&... args) -> element { return create_element("tbody", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto td(Ts&&... args) -> element { return create_element("td", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto tfoot(Ts&&... args) -> element { return create_element("tfoot", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto th(Ts&&... args) -> element { return create_element("th", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto thead(Ts&&... args) -> element { return create_element("thead", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto tr(Ts&&... args) -> element { return create_element("tr", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto button(Ts&&... args) -> element { return create_element("button", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto datalist(Ts&&... args) -> element { return create_element("datalist", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto fieldset(Ts&&... args) -> element { return create_element("fieldset", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto form(Ts&&... args) -> element { return create_element("form", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto input(Ts&&... args) -> element { return create_element("input", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto label(Ts&&... args) -> element { return create_element("label", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto legend(Ts&&... args) -> element { return create_element("legend", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto meter(Ts&&... args) -> element { return create_element("meter", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto optgroup(Ts&&... args) -> element { return create_element("optgroup", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto option(Ts&&... args) -> element { return create_element("option", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto progress(Ts&&... args) -> element { return create_element("progress", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto select(Ts&&... args) -> element { return create_element("select", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto textarea(Ts&&... args) -> element { return create_element("textarea", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto details(Ts&&... args) -> element { return create_element("details", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto dialog(Ts&&... args) -> element { return create_element("dialog", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto summary(Ts&&... args) -> element { return create_element("summary", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto slot(Ts&&... args) -> element { return create_element("slot", std::forward<Ts>(args)...); }
   template<typename ... Ts> auto template_(Ts&&... args) -> element { return create_element("template", std::forward<Ts>(args)...); }


   namespace detail
   {

      [[nodiscard]] auto to_string(const attribute& attrib) -> std::string
      {
         const auto visitor = []<typename T>(const T & alternative) -> std::string
         {
            if constexpr (std::same_as<T, bool_attribute>)
            {
               // The presence of a boolean string_attribute on an element represents the true
               // value, and the absence of the string_attribute represents the false value.
               // [...]]
               // The values "true" and "false" are not allowed on boolean attributes.
               // To represent a false value, the string_attribute has to be omitted altogether.
               // [https://html.spec.whatwg.org/dev/common-microsyntaxes.html#boolean-attributes]
               if (alternative.m_value == false)
                  return {};
               return alternative.m_name;
            }
            else if constexpr (std::same_as<T, string_attribute>)
            {
               return std::format("{}=\"{}\"", alternative.m_name, alternative.m_data);
            }
         };
         return std::visit(visitor, attrib);
      }


      [[nodiscard]] auto get_attribute_str(const element& elem) -> std::string
      {
         const auto visitor = []<typename T>(const T & alternative)
         {
            return to_string(alternative);
         };

         std::string result;
         for (const auto& x : elem.m_attributes)
         {
            result += ' ';
            result += std::visit(visitor, x);
         }
         return result;
      }


      constexpr int indent = 3; // TODO make this parameter

      [[nodiscard]] auto get_spaces(const int count) -> std::string
      {
         std::string result;
         result.reserve(count);
         for (int i = 0; i < count; ++i)
            result += ' ';
         return result;
      }


      [[nodiscard]] auto get_element_str(const std::string& elem, const int level) -> std::string
      {
         return std::format("{}{}", get_spaces(level * indent), elem);
      }

      template<typename variant_type, typename visitor_type>
      [[nodiscard]] auto get_joined_visits(
         const std::vector<variant_type>& variants,
         const visitor_type& visitor,
         const std::string_view delim
      ) -> std::string
      {
         if (variants.empty())
            return std::string{};
         std::string result;
         for (int i = 0; i < std::ssize(variants); ++i)
         {
            if (i > 0)
               result += '\n';
            result += std::visit(visitor, variants[i]);
         }
         return result;
      }


      [[nodiscard]] auto get_inner_html_str(const element& elem, const int level) -> std::string
      {
         const auto content_visitor = [&]<typename T>(const T& alternative) -> std::string
         {
            return get_element_str(alternative, level + 1);
         };

         return get_joined_visits(elem.m_inner_html, content_visitor, "\n");
      }
   } // namespace detail


   [[nodiscard]] auto get_element_str(const element& elem, const int level = 0) -> std::string;

} // namespace cheap


#ifdef CHEAP_IMPL
[[nodiscard]] auto cheap::get_element_str(const element& elem, const int level) -> std::string
{
   if (const auto& trivial_content = elem.get_trivial(); trivial_content.has_value())
   {
      return std::format(
         "{}<{}{}>{}</{}>",
         detail::get_spaces(level * detail::indent),
         elem.m_type,
         detail::get_attribute_str(elem),
         trivial_content.value(),
         elem.m_type
      );
   }
   else
   {
      std::string result;
      result += std::format("<{}{}>", elem.m_type, detail::get_attribute_str(elem));
      result += '\n';
      result += detail::get_inner_html_str(elem, level);
      result += '\n';
      result += std::format("</{}>", elem.m_type);
      return result;
   }
}
#endif
