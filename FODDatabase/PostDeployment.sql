USE [FODDatabase]

IF NOT EXISTS (SELECT 1 FROM [dbo].[User] WHERE Id = 1)
BEGIN
    INSERT INTO [dbo].[User] (Id, Username, Password)
    VALUES (1, 'admin', '393391570616696232');
END