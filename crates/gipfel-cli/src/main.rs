//! gipfel-cli: headless command line tools, mirroring the C binary's
//! `-p/--export-position`, `-e/--export-hills` and stitch modes.

use gipfel_core::hill::FLAG_EXPORT;
use gipfel_core::metadata::{apply_image_metadata, image_dimensions, ImageMetaData};
use gipfel_core::panorama::Panorama;
use gipfel_core::stitch::{self, ScanMode};

fn usage() {
    eprintln!(
        "usage: gipfel-cli <command> [options]\n\
         \n\
         commands:\n\
         \x20 export-position <img>                          print position CSV line\n\
         \x20 export-hills <img> [-d <data>] [-e <hills>]    export hill positions\n\
         \x20 info <img>                                     show parsed metadata\n\
         \x20 stitch [-t out.tif [-4]] [-j out.jpg] [-w N]\n\
         \x20   [-h N] [-r from,to] [-b] img...              cylindrical panorama\n\
         \x20 profiles                                       list distortion profiles\n\
         \x20 profile-save <name> <k0> <k1> <x0>             save a profile\n"
    );
}

fn export_position(img: &str) -> i32 {
    // C reads metadata directly (no image decode); missing values print as nan
    let mut md = ImageMetaData::new();
    md.load_image(img);

    // C: printf(",%s,,%f,%f,%d\n", img_file, latitude, longitude, (int)rint(height))
    println!(
        ",{},,{},{},{}",
        img,
        fmt_f(md.latitude),
        fmt_f(md.longitude),
        md.height.round_ties_even() as i64
    );
    0
}

fn fmt_f(v: f64) -> String {
    if v.is_nan() {
        "nan".to_string()
    } else {
        format!("{:.6}", v)
    }
}

fn export_hills(img: &str, data: &str, export_file: Option<&str>) -> i32 {
    let Some((w, h)) = image_dimensions(img) else {
        eprintln!("could not read image {}", img);
        return 1;
    };

    let mut md = ImageMetaData::new();
    md.load_image(img);

    let mut pan = Panorama::new();
    let have_info = apply_image_metadata(&mut pan, &md, w, h);

    if let Err(e) = pan.load_data(data) {
        eprintln!("Could not load datafile {}: {}", data, e);
        return 1;
    }
    pan.set_height_dist_ratio(0.07); // C default "visibility"

    if !have_info {
        eprintln!("No gipfel info available for {}.", img);
        return 0;
    }

    // -e: restrict export to the hills from the given file
    if let Some(file) = export_file {
        match gipfel_core::hill::load(file) {
            Ok(mut extra) => {
                for m in extra.iter_mut() {
                    m.flags |= FLAG_EXPORT;
                }
                pan.add_hills(&extra);
            }
            Err(_) => return 1,
        }
    }

    println!("#\n# name\theight\tx\ty\tdistance\tflags\n#");

    for row in pan.export_hill_rows(w as i64, h as i64, export_file.is_some()) {
        println!("{}", row);
    }

    if export_file.is_some() {
        for m in pan.hills.iter_mut() {
            m.flags &= !FLAG_EXPORT;
        }
    }

    0
}

/// Port of the C stitch mode: resample the given images into one
/// cylindrical panorama. Flags follow the C binary: -w/-h output size,
/// -r from,to azimuth range in degrees, -b bicubic, -4 16-bit TIFF,
/// -j/-t output file (tiff wins if both given, as in C).
fn stitch(imgs: &[String], opts: &StitchOpts) -> i32 {
    if imgs.is_empty() || (opts.tiff.is_none() && opts.jpeg.is_none()) {
        usage();
        return 2;
    }
    let mut sources = Vec::new();
    for img in imgs {
        match stitch::StitchSource::load(img) {
            Ok(s) => sources.push(s),
            Err(e) => {
                eprintln!("{e}");
                return 1;
            }
        }
    }
    let mode = if opts.bicubic { ScanMode::Bicubic } else { ScanMode::Nearest };
    let out = stitch::resample(&sources, mode, opts.w, opts.h, opts.from, opts.to);
    let hit = out.hit.iter().filter(|h| **h).count();
    eprintln!(
        "stitched {}x{}: {}/{} pixels covered",
        out.w, out.h, hit, out.hit.len()
    );
    let res = if let Some(path) = &opts.tiff {
        stitch::write_tiff(path, &out, opts.bits16)
    } else {
        stitch::write_jpeg(opts.jpeg.as_ref().unwrap(), &out, 90)
    };
    match res {
        Ok(()) => 0,
        Err(e) => {
            eprintln!("{e}");
            1
        }
    }
}

struct StitchOpts {
    w: u32,
    h: u32,
    from: f64,
    to: f64,
    bicubic: bool,
    bits16: bool,
    tiff: Option<String>,
    jpeg: Option<String>,
}

/// C defaults: stitch_w=2000, stitch_h=500, stitch_from=0.0, stitch_to=380.0
fn stitch_defaults() -> StitchOpts {
    StitchOpts {
        w: 2000,
        h: 500,
        from: 0.0,
        to: 380.0,
        bicubic: false,
        bits16: false,
        tiff: None,
        jpeg: None,
    }
}

fn stitch_cmd(args: &[String]) -> i32 {
    let mut o = stitch_defaults();
    let mut imgs = Vec::new();
    let mut i = 0;
    while i < args.len() {
        let a = args[i].as_str();
        let need = |i: &mut usize| -> Option<String> {
            if *i + 1 < args.len() {
                *i += 1;
                Some(args[*i].clone())
            } else {
                None
            }
        };
        let ok = match a {
            "-w" => match need(&mut i) {
                Some(v) => {
                    o.w = v.parse().unwrap_or(2000);
                    true
                }
                None => false,
            },
            "-h" => match need(&mut i) {
                Some(v) => {
                    o.h = v.parse().unwrap_or(500);
                    true
                }
                None => false,
            },
            "-r" => match need(&mut i) {
                Some(v) => {
                    // C: atof(optarg) [, atof(after comma)] — atof reads the
                    // numeric prefix before the comma
                    let (first, second) = v.split_once(',').unwrap_or((v.as_str(), ""));
                    o.from = first.trim().parse().unwrap_or(0.0);
                    if !second.trim().is_empty() {
                        o.to = second.trim().parse().unwrap_or(380.0);
                    }
                    true
                }
                None => false,
            },
            "-j" => match need(&mut i) {
                Some(v) => {
                    o.jpeg = Some(v);
                    true
                }
                None => false,
            },
            "-t" => match need(&mut i) {
                Some(v) => {
                    o.tiff = Some(v);
                    true
                }
                None => false,
            },
            "-b" => {
                o.bicubic = true;
                true
            }
            "-4" => {
                o.bits16 = true;
                true
            }
            _ => {
                if a.starts_with('-') {
                    false
                } else {
                    imgs.push(a.to_string());
                    true
                }
            }
        };
        if !ok {
            usage();
            return 2;
        }
        i += 1;
    }
    stitch(&imgs, &o)
}

fn info(img: &str) -> i32 {
    let mut md = ImageMetaData::new();
    md.load_image(img);
    println!("manufacturer:        {:?}", md.manufacturer);
    println!("model:               {:?}", md.model);
    println!("longitude:           {}", md.longitude);
    println!("latitude:            {}", md.latitude);
    println!("height:              {}", md.height);
    println!("direction:           {}", md.direction);
    println!("pitch:               {}", md.pitch);
    println!("roll:                {}", md.roll);
    println!("focal_length:        {}", md.focal_length);
    println!("focal_length_35mm:   {}", md.focal_length_35mm);
    println!("projection type:     {}", md.projection_type);
    println!("k0:                  {}", md.k0);
    println!("k1:                  {}", md.k1);
    println!("x0:                  {}", md.x0);
    0
}

fn profiles() -> i32 {
    let all = gipfel_core::profile::list();
    if all.is_empty() {
        eprintln!("no profiles in {}", gipfel_core::profile::prefs_path().display());
    }
    for (name, p) in all {
        println!("{name}\tk0={}\tk1={}\tx0={}", p.k0, p.k1, p.x0);
    }
    0
}

fn profile_save(name: &str, k0: f64, k1: f64, x0: f64) -> i32 {
    let p = gipfel_core::profile::Profile { k0, k1, x0 };
    match gipfel_core::profile::save(name, p, true) {
        Ok(()) => {
            eprintln!("saved profile {name}");
            0
        }
        Err(e) => {
            eprintln!("{e}");
            1
        }
    }
}

fn profile_save_cmd(args: &[String]) -> i32 {
    if args.len() != 4 {
        usage();
        return 2;
    }
    let (Ok(k0), Ok(k1), Ok(x0)) = (
        args[1].parse::<f64>(),
        args[2].parse::<f64>(),
        args[3].parse::<f64>(),
    ) else {
        usage();
        return 2;
    };
    profile_save(&args[0], k0, k1, x0)
}

fn main() {
    let args: Vec<String> = std::env::args().collect();
    let Some(cmd) = args.get(1) else {
        usage();
        std::process::exit(2);
    };

    let code = match cmd.as_str() {
        "export-position" => {
            if args.len() != 3 {
                usage();
                2
            } else {
                export_position(&args[2])
            }
        }
        "export-hills" => {
            if args.len() < 3 {
                usage();
                2
            } else {
                let img = &args[2];
                let mut data = "gipfel.dat".to_string();
                let mut export_file: Option<String> = None;
                let mut i = 3;
                while i < args.len() {
                    match args[i].as_str() {
                        "-d" => {
                            if i + 1 < args.len() {
                                data = args[i + 1].clone();
                                i += 1;
                            }
                        }
                        "-e" => {
                            if i + 1 < args.len() {
                                export_file = Some(args[i + 1].clone());
                                i += 1;
                            }
                        }
                        _ => {}
                    }
                    i += 1;
                }
                export_hills(img, &data, export_file.as_deref())
            }
        }
        "info" => {
            if args.len() != 3 {
                usage();
                2
            } else {
                info(&args[2])
            }
        }
        "profiles" => profiles(),
        "profile-save" => profile_save_cmd(&args[2..]),
        "stitch" => stitch_cmd(&args[2..]),
        _ => {
            usage();
            2
        }
    };

    std::process::exit(code);
}
