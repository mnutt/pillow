#include <QtCore/QCoreApplication>
#include <QtCore/QProcess>
#include <QtCore/QDir>
#include <QtCore/QStringList>
#include <iostream>

int main(int argc, char* argv[])
{
	QCoreApplication app(argc, argv);

	// Get the directory where this executable is located
	QString testDir = QCoreApplication::applicationDirPath();

	// List of all test executables
	QStringList testExecutables = {"test_httpclient",
	                               "test_httprequestwriter",
	                               "test_httpresponseparser",
	                               "test_networkaccessmanager",
	                               "test_bytearrayhelpers",
	                               "test_httpheader",
	                               "test_httphandler",
	                               "test_httphandlerfile",
	                               "test_httphandlersimplerouter",
	                               "test_httphandlerproxy",
	                               "test_httpconnection_tcp",
	                               "test_httpconnection_ssl",
	                               "test_httpconnection_local",
	                               "test_httpconnection_buffer",
	                               "test_httpserver",
	                               "test_httplocalserver",
	                               "test_httpsserver"};

	int totalTests = 0;
	int passedTests = 0;
	int failedTests = 0;

	std::cout << "Running Pillow Test Suite" << std::endl;
	std::cout << "========================" << std::endl << std::endl;

	// Run each test executable
	for (const QString& testName : testExecutables)
	{
		QString testPath = QDir(testDir).filePath(testName);

		// Check if test executable exists
		if (!QFile::exists(testPath))
		{
			std::cout << "Warning: Test executable not found: " << testName.toStdString() << std::endl;
			continue;
		}

		std::cout << "Running " << testName.toStdString() << "..." << std::endl;

		QProcess process;
		process.start(testPath, QStringList());

		if (!process.waitForStarted())
		{
			std::cout << "  ERROR: Failed to start test" << std::endl;
			failedTests++;
			totalTests++;
			continue;
		}

		// Wait for test to complete (timeout after 60 seconds)
		if (!process.waitForFinished(60000))
		{
			std::cout << "  ERROR: Test timed out" << std::endl;
			process.terminate();
			failedTests++;
			totalTests++;
			continue;
		}

		totalTests++;

		// Check exit code
		int exitCode = process.exitCode();
		if (exitCode == 0)
		{
			std::cout << "  PASSED" << std::endl;
			passedTests++;
		}
		else
		{
			std::cout << "  FAILED (exit code: " << exitCode << ")" << std::endl;
			failedTests++;

			// Print test output on failure
			QByteArray output = process.readAllStandardOutput();
			QByteArray error = process.readAllStandardError();

			if (!output.isEmpty())
			{
				std::cout << "  Output:" << std::endl;
				std::cout << output.toStdString();
			}

			if (!error.isEmpty())
			{
				std::cout << "  Errors:" << std::endl;
				std::cout << error.toStdString();
			}
		}

		std::cout << std::endl;
	}

	// Print summary
	std::cout << "Test Summary" << std::endl;
	std::cout << "============" << std::endl;
	std::cout << "Total tests run: " << totalTests << std::endl;
	std::cout << "Passed: " << passedTests << std::endl;
	std::cout << "Failed: " << failedTests << std::endl;

	if (failedTests == 0)
	{
		std::cout << std::endl << "All tests passed!" << std::endl;
		return 0;
	}
	else
	{
		std::cout << std::endl << "Some tests failed." << std::endl;
		return 1;
	}
}