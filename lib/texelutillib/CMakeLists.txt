set(src_texelutillib
                 assignment.hpp
  bookbuild.cpp  bookbuild.hpp
  gametree.cpp   gametree.hpp
                 gametreeutil.hpp
  proofgame.cpp  proofgame.hpp
                 stloutput.hpp
                 threadpool.hpp
  tbpath.cpp     tbpath.hpp
  )

add_library(texelutillib STATIC
  ${src_texelutillib}
  )
target_link_libraries(texelutillib
  PUBLIC texellib
  )
target_include_directories(texelutillib
  INTERFACE .
  )
