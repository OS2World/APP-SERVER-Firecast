<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet version="2.0" xmlns:xsl="http://www.w3.org/1999/XSL/Transform">
<xsl:output method="html" encoding="utf-8" indent="yes"/>

<xsl:include href="utils.xsl" />

<xsl:template match="/">
<head>
  <meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
  <meta http-equiv="Content-Script-Type" content="text/javascript" />
  <script type="text/javascript" src="utils.js"></script>
  <script type="text/javascript" src="admin.js"></script>
  <link rel="stylesheet" type="text/css" href="style.css" />
</head>
<body>
  <xsl:call-template name="page-title">
    <xsl:with-param name="image">admin</xsl:with-param>
    <xsl:with-param name="text">Administrator</xsl:with-param>
  </xsl:call-template>

  <div id="admin">
    <xsl:apply-templates select="admin" />
  </div>

  <form enctype="text/plain" action="/admin/admin"
        method="post" name="apply"
        onsubmit=" if ( !ConfirmChanges() )
                     return false;
                   document.apply.XMLText.value = AdminMakeXML( document.getElementById('admin') );
                   return true; ">
    <input type="hidden" name="XMLText" />
    <input type="submit" value="Apply" class="apply" />
  </form>
</body>
</xsl:template>

<xsl:template match="admin">
  <span class="field">Name:</span>
  <input type="text" name="name" size="20" maxlength="20">
    <xsl:attribute name="value">
      <xsl:value-of select="name" />
    </xsl:attribute>
  </input>
  <br />

  <span class="field">Password:</span>
  <input type="password" name="password" size="20" maxlength="20">
    <xsl:attribute name="value">
      <xsl:value-of select="password" />
    </xsl:attribute>
  </input>

  <h2>Access list:</h2>
  <xsl:call-template name="access-list" />

  <h2>Logging</h2>
  level:
  <select name="log-level">
    <option value="0">
      <xsl:if test="log-level=0">
        <xsl:attribute name="selected">1</xsl:attribute>
      </xsl:if>
      Error
    </option>
    <option value="1">
      <xsl:if test="log-level=1">
        <xsl:attribute name="selected">1</xsl:attribute>
      </xsl:if>
      Warning
    </option>
    <option value="2">
      <xsl:if test="log-level=2">
        <xsl:attribute name="selected">1</xsl:attribute>
      </xsl:if>
      Info.
    </option>
    <option value="3">
      <xsl:if test="log-level=3">
        <xsl:attribute name="selected">1</xsl:attribute>
      </xsl:if>
      Detail
    </option>
    <option value="4">
      <xsl:if test="log-level=4">
        <xsl:attribute name="selected">1</xsl:attribute>
      </xsl:if>
      Debug
    </option>
  </select>

  , buffer length:
  <input type="text" name="log-buffer" size="6" maxlength="5">
    <xsl:attribute name="value">
      <xsl:value-of select="log-buffer" />
    </xsl:attribute>
  </input>
  bytes
</xsl:template>

</xsl:stylesheet>
