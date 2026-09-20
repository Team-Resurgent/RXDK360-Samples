namespace Microsoft.ATG.ManagedNUIConduit
{
    partial class ConsumerForm
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
            System.Windows.Forms.Label label1;
            System.Windows.Forms.Label label2;
            this.depthPictureBox = new System.Windows.Forms.PictureBox();
            this.colorPictureBox = new System.Windows.Forms.PictureBox();
            this.returnButton = new System.Windows.Forms.Button();
            this.statusLabel = new System.Windows.Forms.Label();
            label1 = new System.Windows.Forms.Label();
            label2 = new System.Windows.Forms.Label();
            ((System.ComponentModel.ISupportInitialize)(this.depthPictureBox)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.colorPictureBox)).BeginInit();
            this.SuspendLayout();
            // 
            // label1
            // 
            label1.AutoSize = true;
            label1.Location = new System.Drawing.Point(12, 9);
            label1.Name = "label1";
            label1.Size = new System.Drawing.Size(117, 13);
            label1.TabIndex = 0;
            label1.Text = "Depth + Player Index:";
            // 
            // label2
            // 
            label2.AutoSize = true;
            label2.Location = new System.Drawing.Point(338, 9);
            label2.Name = "label2";
            label2.Size = new System.Drawing.Size(38, 13);
            label2.TabIndex = 2;
            label2.Text = "Color + Skeleton:";
            // 
            // depthPictureBox
            // 
            this.depthPictureBox.BackColor = System.Drawing.SystemColors.ControlDark;
            this.depthPictureBox.Location = new System.Drawing.Point(15, 25);
            this.depthPictureBox.Name = "depthPictureBox";
            this.depthPictureBox.Size = new System.Drawing.Size(320, 240);
            this.depthPictureBox.SizeMode = System.Windows.Forms.PictureBoxSizeMode.Zoom;
            this.depthPictureBox.TabIndex = 1;
            this.depthPictureBox.TabStop = false;
            // 
            // colorPictureBox
            // 
            this.colorPictureBox.BackColor = System.Drawing.SystemColors.ControlDark;
            this.colorPictureBox.Location = new System.Drawing.Point(341, 25);
            this.colorPictureBox.Name = "colorPictureBox";
            this.colorPictureBox.Size = new System.Drawing.Size(320, 240);
            this.colorPictureBox.SizeMode = System.Windows.Forms.PictureBoxSizeMode.Zoom;
            this.colorPictureBox.TabIndex = 3;
            this.colorPictureBox.TabStop = false;
            // 
            // returnButton
            // 
            this.returnButton.Location = new System.Drawing.Point(586, 271);
            this.returnButton.Name = "returnButton";
            this.returnButton.Size = new System.Drawing.Size(75, 23);
            this.returnButton.TabIndex = 5;
            this.returnButton.Text = "Return";
            this.returnButton.UseVisualStyleBackColor = true;
            this.returnButton.Click += new System.EventHandler(this.returnButton_Click);
            // 
            // statusLabel
            // 
            this.statusLabel.Location = new System.Drawing.Point(12, 271);
            this.statusLabel.Name = "statusLabel";
            this.statusLabel.Size = new System.Drawing.Size(568, 23);
            this.statusLabel.TabIndex = 4;
            this.statusLabel.Text = "...";
            this.statusLabel.TextAlign = System.Drawing.ContentAlignment.MiddleLeft;
            // 
            // ConsumerForm
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(674, 306);
            this.Controls.Add(this.statusLabel);
            this.Controls.Add(this.returnButton);
            this.Controls.Add(this.colorPictureBox);
            this.Controls.Add(this.depthPictureBox);
            this.Controls.Add(label2);
            this.Controls.Add(label1);
            this.Font = new System.Drawing.Font("Segoe UI", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedSingle;
            this.MaximizeBox = false;
            this.Name = "ConsumerForm";
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
            this.Text = "Managed NUI Conduit - Consumer";
            this.Load += new System.EventHandler(this.ConsumerForm_Load);
            this.FormClosing += new System.Windows.Forms.FormClosingEventHandler(this.ConsumerForm_FormClosing);
            ((System.ComponentModel.ISupportInitialize)(this.depthPictureBox)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.colorPictureBox)).EndInit();
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.PictureBox depthPictureBox;
        private System.Windows.Forms.PictureBox colorPictureBox;
        private System.Windows.Forms.Button returnButton;
        private System.Windows.Forms.Label statusLabel;
    }
}