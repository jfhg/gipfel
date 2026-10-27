mod app;
mod bridge;
mod canvas_view;

fn main() {
    yew::Renderer::<app::App>::new().render();
}
