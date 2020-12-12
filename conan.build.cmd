if "%APPVEYOR_REPO_TAG%" == "true" (
    conan create %APPVEYOR_BUILD_FOLDER%\cmake "%APPVEYOR_PROJECT_NAME%/%APPVEYOR_REPO_TAG_NAME:v=%@barco/healthcare"
    conan upload --remote barco --all %APPVEYOR_PROJECT_NAME%/%APPVEYOR_REPO_TAG_NAME:v=%@barco/healthcare
) else (
    if "%APPVEYOR_REPO_BRANCH%" == "barco_1.12.16" (
        conan create %APPVEYOR_BUILD_FOLDER%\cmake %APPVEYOR_PROJECT_NAME%/1.12.16.1@barco/healthcare
        conan upload --remote barco --all --force %APPVEYOR_PROJECT_NAME%/1.12.16.1@barco/healthcare
    )
)
