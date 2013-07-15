{
  'targets': [
    {
      'target_name': 'latency-benchmark',
      'type': 'executable',
      'sources': [
        'src/latency-benchmark.c',
        'src/latency-benchmark.h',
        'src/screenscraper.h',
        'src/server.c',
        '<(INTERMEDIATE_DIR)/packaged-html-files.c',
      ],
      'dependencies': [
        'mongoose',
      ],
      'actions': [
        {
          'action_name': 'package html files',
          'inputs': [
            'files-to-c-arrays.py',
            'html/2048.png',
            'html/compatibility.js',
            'html/draw-pattern.js',
            'html/gradient.png',
            'html/index.html',
            'html/keep-server-alive.js',
            'html/latency-benchmark.css',
            'html/latency-benchmark.html',
            'html/latency-benchmark.js',
            'html/worker.js',
          ],
          'outputs': [
            '<(INTERMEDIATE_DIR)/packaged-html-files.c',
          ],
          'action': ['python', 'files-to-c-arrays.py', '<@(_outputs)', '<@(_inputs)'],
          'msvs_cygwin_shell': 0,
        },
      ],
      'conditions': [
        ['OS=="linux"', {
          'sources': [
            'src/x11/screenscraper.c',
            'src/x11/main.c',
          ],
        }],
        ['OS=="win"', {
          'sources': [
            'src/win/main.cpp',
            'src/win/screenscraper.cpp',
			'src/win/stdafx.h',
          ],
        }],
        ['OS=="mac"', {
          'sources': [
            'src/mac/main.m',
            'src/mac/screenscraper.m',
          ],
          'link_settings': {
            'libraries': [
              '$(SDKROOT)/System/Library/Frameworks/Cocoa.framework',
              '$(SDKROOT)/System/Library/Frameworks/CoreVideo.framework',
              '$(SDKROOT)/System/Library/Frameworks/OpenGL.framework',
            ],
          },
        }],
      ],
      'msvs_settings': {
        'VCCLCompilerTool': {
          'CompileAs': 2, # Compile C as C++, since msvs doesn't support C99
        },
      },
    },
    {
      'target_name': 'mongoose',
      'type': 'static_library',
      'sources': [
        'third_party/mongoose/mongoose.c',
        'third_party/mongoose/mongoose.h',
      ],
    },
  ],

  'target_defaults': {
    'default_configuration': 'Debug',
    'configurations': {
      'Debug': {
        'defines': [ 'DEBUG', '_DEBUG' ],
        'cflags': [ '-g', '-O0' ],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'RuntimeLibrary': 1, # static debug
            'Optimization': 0, # optimizeDisabled (/Od)
          },
        },
        'xcode_settings': {'GCC_OPTIMIZATION_LEVEL': 0 } # -O0 No optimization
      },
      'Release': {
        'defines': [ 'NDEBUG' ],
        'cflags': [ '-Os' ],
        'msvs_settings': {
          'VCCLCompilerTool': {
            'RuntimeLibrary': 0, # static release
            'Optimization': 3, # full optimization (/Ox)
          },
        },
        'xcode_settings': { 'GCC_OPTIMIZATION_LEVEL': 's' }, # -Os optimize for size+speed
      }
    },
    'conditions': [
      ['OS=="linux"', {
        'link_settings': {
          'libraries' : [
          '-ldl',
          '-lX11',
          '-lXtst',
          '-lGL',
          ],
        },
      }],
      ['OS=="win"', {
        'defines': [ '_WINDOWS', 'WIN32' ],
      }],
      ['OS=="mac"', {
        'xcode_settings': {
          'ARCHS': '$(ARCHS_STANDARD_64_BIT)',
        },
      }],
    ],
    'msvs_settings': {
      'VCLinkerTool': {
        'GenerateDebugInformation': 'true',
        'SubSystem': 2, # Windows
      },
    },
    'xcode_settings': {
      # This prevents a silly warning in XCode's UI ("Update project to recommended settings"). Unfortunately there's no way to prevent the warning on 'action' targets; oh well.
      'COMBINE_HIDPI_IMAGES': 'YES',
    },
  },

  'conditions': [
    ['OS=="mac"',
      # The XCode project generator automatically adds a bogus "All" target with bad xcode_settings unless we define one here.
      {
        'targets': [
          {
            'target_name': 'All',
            'type': 'static_library',
          },
        ],
      },
    ],
  ],
}
