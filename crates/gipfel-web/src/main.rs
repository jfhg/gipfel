mod app;
mod bridge;
mod canvas_view;
mod compass;
mod markers;
mod widgets;

fn main() {
    yew::Renderer::<app::App>::new().render();
}
