use gipfel_core::panorama::Panorama;
fn main() {
    let mut p = Panorama::new();
    p.set_center_angle(400.0); // "bearing + full turn" input slip
    println!("az=400°: hill at alph=-180° (220° off, behind) visible? {}", p.is_visible(-std::f64::consts::PI));
    println!("az=NaN:  visible? {}", p.is_visible(f64::NAN));
    println!("az=inf:  visible? {}", p.is_visible(f64::INFINITY));
    p.set_center_angle(f64::NAN);
    println!("center=NaN: everything visible? {}", p.is_visible(0.0));
}
