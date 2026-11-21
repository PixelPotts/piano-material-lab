$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
$form=New-Object Windows.Forms.Form
$form.Text='C Piano - Audio API setup'
$form.ClientSize=New-Object Drawing.Size(500,220)
$form.StartPosition='CenterScreen'
$form.FormBorderStyle='FixedDialog'
$form.MaximizeBox=$false
$label=New-Object Windows.Forms.Label
$label.Location=New-Object Drawing.Point(18,15)
$label.Size=New-Object Drawing.Size(465,65)
$label.Text="Enter an OpenAI API key to enable WAV listening assessments.`r`nSaved encrypted for your Windows account. API usage is billed`r`nthrough your API project. The key stays out of chat and source files."
$form.Controls.Add($label)
$link=New-Object Windows.Forms.LinkLabel
$link.Text='Create an API key'
$link.Location=New-Object Drawing.Point(18,83)
$link.AutoSize=$true
$link.Add_LinkClicked({Start-Process 'https://platform.openai.com/api-keys'})
$form.Controls.Add($link)
$box=New-Object Windows.Forms.TextBox
$box.Location=New-Object Drawing.Point(18,111)
$box.Size=New-Object Drawing.Size(464,25)
$box.UseSystemPasswordChar=$true
$form.Controls.Add($box)
$save=New-Object Windows.Forms.Button
$save.Text='Save key'
$save.Location=New-Object Drawing.Point(277,164)
$save.Size=New-Object Drawing.Size(100,30)
$save.Add_Click({
 if ([string]::IsNullOrWhiteSpace($box.Text)) { return }
 try {
  $dir=Join-Path $env:LOCALAPPDATA 'CPiano'
  [void](New-Item -ItemType Directory -Force -Path $dir)
  $secure=ConvertTo-SecureString $box.Text.Trim() -AsPlainText -Force
  $credential=New-Object Management.Automation.PSCredential('OpenAI',$secure)
  $credential | Export-Clixml (Join-Path $dir 'api-key.xml')
  $box.Clear()
  $form.DialogResult=[Windows.Forms.DialogResult]::OK
  $form.Close()
 } catch { [void][Windows.Forms.MessageBox]::Show('Could not save the key. Check folder access.','Audio setup') }
})
$form.Controls.Add($save)
$cancel=New-Object Windows.Forms.Button
$cancel.Text='Cancel'
$cancel.Location=New-Object Drawing.Point(384,164)
$cancel.Size=New-Object Drawing.Size(98,30)
$cancel.DialogResult=[Windows.Forms.DialogResult]::Cancel
$form.Controls.Add($cancel)
$form.AcceptButton=$save
$form.CancelButton=$cancel
$result=$form.ShowDialog()
$box.Clear()
$form.Dispose()
if ($result -eq [Windows.Forms.DialogResult]::OK) { exit 0 } else { exit 2 }
