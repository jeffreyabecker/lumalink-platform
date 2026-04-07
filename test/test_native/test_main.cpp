#include <unity.h>

#include <LumaLinkPlatform.h>

void setUp()
{
}

void tearDown()
{
}

void test_project_name_is_exposed()
{
    TEST_ASSERT_EQUAL_STRING("lumalink-platform", lumalink::platform::ProjectName.data());
}

void test_contract_names_are_exposed()
{
    TEST_ASSERT_EQUAL_STRING("buffers", lumalink::platform::buffers::ContractName.data());
    TEST_ASSERT_EQUAL_STRING("transport", lumalink::platform::transport::ContractName.data());
    TEST_ASSERT_EQUAL_STRING("filesystem", lumalink::platform::filesystem::ContractName.data());
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    UNITY_BEGIN();
    RUN_TEST(test_project_name_is_exposed);
    RUN_TEST(test_contract_names_are_exposed);
    return UNITY_END();
}
