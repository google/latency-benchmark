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
      ],
      'dependencies': [
        'mongoose',
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
            'Optimization': '0', # optimizeDisabled (/Od)
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
            'Optimization': '3', # full optimization (/Ox)
          },
        },
        'xcode_settings': { 'GCC_OPTIMIZATION_LEVEL': 's' } # -Os optimize for size+speed
      }
    },
    'conditions': [
      ['OS=="linux"', {
        'ldflags': [
          '-pthread',
        ],
        'link_settings': {
          'libraries' : [
          '-ldl',
          '-lX11',
          '-lXtst',
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
        'SubSystem': '2' # Windows
      },
    },
    'xcode_settings': {
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
