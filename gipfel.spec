Name: gipfel
Summary: gipfel - Photogrammetry For Mountain Images
Version: 0.3.0
Release: 3.1
URL: http://www.ecademix.com/JohannesHofmann/gipfel.html
BuildRoot:      %{_tmppath}/%{name}-%{version}-build
Source0: %{name}-%{version}.tar.gz
License: GPL
Group: Productivity/Scientific/Other

BuildRequires: binutils gcc gcc-c++ libstdc++-devel gsl-devel fltk-devel libjpeg-devel libtiff-devel libpng-devel exiv2-devel

%if 0%{?suse_version}
BuildRequires: xorg-x11-libXext-devel
%endif

%description
gipfel helps to find the names of mountains or points of interest
on a picture.
It uses a database containing names and GPS data. With the given viewpoint
(the point from which the picture was taken) and two known mountains
on the picture, gipfel can compute all parameters needed to compute the
positions of other mountains on the picture.
gipfel can also be used to play around with the parameters manually.
%prep
%setup -q
%build
./configure --prefix=%{_prefix} --docdir=%{_defaultdocdir}/%{name} || cat config.log
make
%install
make DESTDIR=$RPM_BUILD_ROOT install-strip
rm -rf $RPM_BUILD_ROOT/%{_defaultdocdir}/%{name}
%files
%defattr(-, root, root)
%doc NEWS README
%{_bindir}/%{name}
%dir %{_datadir}/%{name}
%{_datadir}/%{name}/gipfel.dat

%changelog
* Tue Apr 7 2009 Johannes Hofmann <Johannes.Hofmann@gmx.de>
- update to gipfel-0.3.0
* Thu Mar 26 2009 Johannes Hofmann <Johannes.Hofmann@gmx.de>
- update to gipfel-0.2.9
* Sun Mar 22 2009 Johannes Hofmann <Johannes.Hofmann@gmx.de>
- initial revision
