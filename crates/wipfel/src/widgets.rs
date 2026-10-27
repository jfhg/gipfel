//! Small form controls: full-width slider with value readout above
//! (mirrors the desktop `slider_item` layout) and a numeric input row.

use web_sys::{HtmlInputElement, InputEvent};
use yew::prelude::*;

#[derive(Properties, PartialEq)]
pub struct SliderProps {
    pub label: AttrValue,
    /// formatted value shown right of the label
    pub display: AttrValue,
    pub value: f64,
    pub min: f64,
    pub max: f64,
    pub step: f64,
    pub onchange: Callback<f64>,
}

#[function_component(SliderItem)]
pub fn slider_item(p: &SliderProps) -> Html {
    // live update while dragging (oninput), not only on release
    let oninput = {
        let onchange = p.onchange.clone();
        Callback::from(move |e: InputEvent| {
            let input: HtmlInputElement = e.target_unchecked_into();
            if let Ok(v) = input.value().parse::<f64>() {
                onchange.emit(v);
            }
        })
    };
    html! {
        <div class="slider">
            <div class="sliderhead">
                <span>{ p.label.as_ref().to_string() }</span>
                <span class="val">{ p.display.as_ref().to_string() }</span>
            </div>
            <input
                type="range"
                min={p.min.to_string()}
                max={p.max.to_string()}
                step={p.step.to_string()}
                value={p.value.to_string()}
                {oninput}
            />
        </div>
    }
}

/// single-line text row (e.g. paste a GPS string)
#[derive(Properties, PartialEq)]
pub struct TextProps {
    pub label: AttrValue,
    pub placeholder: AttrValue,
    pub onchange: Callback<String>,
}

#[function_component(TextItem)]
pub fn text_item(p: &TextProps) -> Html {
    let onchange = {
        let onchange = p.onchange.clone();
        Callback::from(move |e: Event| {
            let input: HtmlInputElement = e.target_unchecked_into();
            onchange.emit(input.value());
        })
    };
    html! {
        <label class="numrow">
            <span>{ p.label.as_ref().to_string() }</span>
            <input
                type="text"
                placeholder={p.placeholder.as_ref().to_string()}
                {onchange}
            />
        </label>
    }
}

#[derive(Properties, PartialEq)]
pub struct NumProps {
    pub label: AttrValue,
    pub value: f64,
    pub step: f64,
    #[prop_or(2)]
    pub decimals: usize,
    pub onchange: Callback<f64>,
}

#[function_component(NumItem)]
pub fn num_item(p: &NumProps) -> Html {
    let onchange = {
        let onchange = p.onchange.clone();
        Callback::from(move |e: Event| {
            let input: HtmlInputElement = e.target_unchecked_into();
            if let Ok(v) = input.value().parse::<f64>() {
                onchange.emit(v);
            }
        })
    };
    let display = format!("{:.*}", p.decimals, p.value);
    html! {
        <label class="numrow">
            <span>{ p.label.as_ref().to_string() }</span>
            <input type="number" step={p.step.to_string()} value={display} {onchange} />
        </label>
    }
}
