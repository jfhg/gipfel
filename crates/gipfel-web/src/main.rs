mod app;
mod bridge;
mod canvas_view;
mod markers;

fn main() {
    yew::Renderer::<app::App>::new().render();
}
