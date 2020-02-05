
project('chex-game', 'c',
		default_options: [
		'buildtype=debugoptimized',
		'warning_level=1',
		])

src = ['hex-game.c', 'weighted-quick-union.c']
cc = meson.get_compiler('c')
deps = []

if get_option('buildtype') == 'debug'
	deps += cc.find_library('allegro-debug')
	deps += cc.find_library('allegro_primitives-debug')
	deps += cc.find_library('allegro_font-debug')
	deps += cc.find_library('allegro_ttf-debug')
	deps += cc.find_library('allegro_image-debug')
else
	deps += cc.find_library('allegro')
	deps += cc.find_library('allegro_primitives')
	deps += cc.find_library('allegro_font')
	deps += cc.find_library('allegro_ttf')
	deps += cc.find_library('allegro_image')
endif


configure_file(input: 'icon.png', output: 'icon.png', copy: true)
configure_file(input: 'VCR_OSD_MONO_1.001.ttf', output: 'VCR_OSD_MONO_1.001.ttf', copy: true)


executable('chex-game', src, dependencies: deps)			
