add_rules("mode.debug", "mode.release")
set_languages("c17")
set_warnings("allextra")

target("wf")
    set_kind("static")
    add_files("src/*.c")
    add_includedirs("include", {public = true})

target("wordfreq")
    set_kind("binary")
    add_deps("wf")
    add_files("app/main.c")
