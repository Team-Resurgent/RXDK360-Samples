namespace Microsoft.ATG.ManagedNUIConduit
{
    partial class RecordPlaybackForm
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.components = new System.ComponentModel.Container();
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(RecordPlaybackForm));
            System.Windows.Forms.Label label1;
            this.recordButton = new System.Windows.Forms.Button();
            this.imageList = new System.Windows.Forms.ImageList(this.components);
            this.stopButton = new System.Windows.Forms.Button();
            this.playButton = new System.Windows.Forms.Button();
            this.returnButton = new System.Windows.Forms.Button();
            this.statusLabel = new System.Windows.Forms.Label();
            this.saveFileDialog = new System.Windows.Forms.SaveFileDialog();
            this.colorCheckBox = new System.Windows.Forms.CheckBox();
            this.depthCheckBox = new System.Windows.Forms.CheckBox();
            label1 = new System.Windows.Forms.Label();
            this.SuspendLayout();
            // 
            // recordButton
            // 
            this.recordButton.ImageAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.recordButton.ImageKey = "Record.bmp";
            this.recordButton.ImageList = this.imageList;
            this.recordButton.Location = new System.Drawing.Point(239, 12);
            this.recordButton.Name = "recordButton";
            this.recordButton.Size = new System.Drawing.Size(137, 23);
            this.recordButton.TabIndex = 0;
            this.recordButton.Text = "Record...";
            this.recordButton.UseVisualStyleBackColor = true;
            this.recordButton.Click += new System.EventHandler(this.recordButton_Click);
            // 
            // imageList
            // 
            this.imageList.ImageStream = ((System.Windows.Forms.ImageListStreamer)(resources.GetObject("imageList.ImageStream")));
            this.imageList.TransparentColor = System.Drawing.Color.Fuchsia;
            this.imageList.Images.SetKeyName(0, "Play.bmp");
            this.imageList.Images.SetKeyName(1, "Record.bmp");
            this.imageList.Images.SetKeyName(2, "Stop.bmp");
            // 
            // stopButton
            // 
            this.stopButton.ImageAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.stopButton.ImageKey = "Stop.bmp";
            this.stopButton.ImageList = this.imageList;
            this.stopButton.Location = new System.Drawing.Point(382, 12);
            this.stopButton.Name = "stopButton";
            this.stopButton.Size = new System.Drawing.Size(137, 23);
            this.stopButton.TabIndex = 4;
            this.stopButton.Text = "Stop";
            this.stopButton.UseVisualStyleBackColor = true;
            this.stopButton.Click += new System.EventHandler(this.stopButton_Click);
            // 
            // playButton
            // 
            this.playButton.ImageAlign = System.Drawing.ContentAlignment.MiddleLeft;
            this.playButton.ImageKey = "Play.bmp";
            this.playButton.ImageList = this.imageList;
            this.playButton.Location = new System.Drawing.Point(525, 12);
            this.playButton.Name = "playButton";
            this.playButton.Size = new System.Drawing.Size(137, 23);
            this.playButton.TabIndex = 5;
            this.playButton.Text = "Play";
            this.playButton.UseVisualStyleBackColor = true;
            this.playButton.Click += new System.EventHandler(this.playButton_Click);
            // 
            // returnButton
            // 
            this.returnButton.Location = new System.Drawing.Point(587, 65);
            this.returnButton.Name = "returnButton";
            this.returnButton.Size = new System.Drawing.Size(75, 23);
            this.returnButton.TabIndex = 7;
            this.returnButton.Text = "Return";
            this.returnButton.UseVisualStyleBackColor = true;
            this.returnButton.Click += new System.EventHandler(this.returnButton_Click);
            // 
            // statusLabel
            // 
            this.statusLabel.Location = new System.Drawing.Point(12, 65);
            this.statusLabel.Name = "statusLabel";
            this.statusLabel.Size = new System.Drawing.Size(568, 23);
            this.statusLabel.TabIndex = 6;
            this.statusLabel.Text = "...";
            this.statusLabel.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            // 
            // saveFileDialog
            // 
            this.saveFileDialog.DefaultExt = "xed";
            this.saveFileDialog.Filter = "XED Files|*.xed|All Files|*.*";
            this.saveFileDialog.Title = "Record Event Data";
            // 
            // label1
            // 
            label1.AutoSize = true;
            label1.Location = new System.Drawing.Point(12, 17);
            label1.Name = "label1";
            label1.Size = new System.Drawing.Size(76, 13);
            label1.TabIndex = 0;
            label1.Text = "Stream Types:";
            // 
            // colorCheckBox
            // 
            this.colorCheckBox.AutoSize = true;
            this.colorCheckBox.Location = new System.Drawing.Point(94, 16);
            this.colorCheckBox.Name = "colorCheckBox";
            this.colorCheckBox.Size = new System.Drawing.Size(54, 17);
            this.colorCheckBox.TabIndex = 1;
            this.colorCheckBox.Text = "Color";
            this.colorCheckBox.UseVisualStyleBackColor = true;
            // 
            // depthCheckBox
            // 
            this.depthCheckBox.AutoSize = true;
            this.depthCheckBox.Location = new System.Drawing.Point(154, 16);
            this.depthCheckBox.Name = "depthCheckBox";
            this.depthCheckBox.Size = new System.Drawing.Size(58, 17);
            this.depthCheckBox.TabIndex = 2;
            this.depthCheckBox.Text = "Depth";
            this.depthCheckBox.UseVisualStyleBackColor = true;
            // 
            // RecordPlaybackForm
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(674, 100);
            this.Controls.Add(this.depthCheckBox);
            this.Controls.Add(this.colorCheckBox);
            this.Controls.Add(label1);
            this.Controls.Add(this.statusLabel);
            this.Controls.Add(this.returnButton);
            this.Controls.Add(this.playButton);
            this.Controls.Add(this.stopButton);
            this.Controls.Add(this.recordButton);
            this.Font = new System.Drawing.Font("Segoe UI", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedSingle;
            this.MaximizeBox = false;
            this.Name = "RecordPlaybackForm";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
            this.Text = "Managed NUI Conduit - Record/Playback";
            this.Load += new System.EventHandler(this.RecordPlaybackForm_Load);
            this.FormClosing += new System.Windows.Forms.FormClosingEventHandler(this.RecordPlaybackForm_FormClosing);
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.Button recordButton;
        private System.Windows.Forms.ImageList imageList;
        private System.Windows.Forms.Button stopButton;
        private System.Windows.Forms.Button playButton;
        private System.Windows.Forms.Button returnButton;
        private System.Windows.Forms.Label statusLabel;
        private System.Windows.Forms.SaveFileDialog saveFileDialog;
        private System.Windows.Forms.CheckBox colorCheckBox;
        private System.Windows.Forms.CheckBox depthCheckBox;
    }
}